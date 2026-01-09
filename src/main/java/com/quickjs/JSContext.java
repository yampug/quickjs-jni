package com.quickjs;

import java.lang.ref.Cleaner;

public class JSContext implements AutoCloseable {
    long ptr;
    private final JSRuntime runtime;

    private final Cleaner.Cleanable cleanable;

    JSContext(long ptr, JSRuntime runtime) {
        this.ptr = ptr;
        this.runtime = runtime;
        this.cleanable = QuickJS.cleaner.register(this, new NativeContextCleaner(ptr));
        registerJavaContext(ptr, this);
    }

    public static final int EVAL_TYPE_GLOBAL = 0;
    public static final int EVAL_TYPE_MODULE = 1;

    public JSValue eval(String script) {
        return eval(script, "<input>", EVAL_TYPE_GLOBAL);
    }

    public JSValue eval(String script, String fileName, int type) {
        runtime.checkThread();
        checkClosed();
        long valPtr = evalInternal(ptr, script, fileName, type);
        return new JSValue(valPtr, this);
    }

    public JSValue parseJSON(String json) {
        runtime.checkThread();
        checkClosed();
        long valPtr = parseJSONInternal(ptr, json);
        return new JSValue(valPtr, this);
    }

    public JSValue createFunction(JSFunction callback, String name, int argCount) {
        runtime.checkThread();
        checkClosed();
        long valPtr = createFunctionInternal(ptr, callback, name, argCount);
        return new JSValue(valPtr, this);
    }

    public JSValue createInteger(int value) {
        runtime.checkThread();
        checkClosed();
        long valPtr = createIntegerInternal(ptr, value);
        return new JSValue(valPtr, this);
    }

    public JSValue createString(String value) {
        runtime.checkThread();
        checkClosed();
        long valPtr = createStringInternal(ptr, value);
        return new JSValue(valPtr, this);
    }

    public JSValue getGlobalObject() {
        runtime.checkThread();
        checkClosed();
        long valPtr = getGlobalObjectInternal(ptr);
        return new JSValue(valPtr, this);
    }

    public void setGlobal(String key, JSValue value) {
        try (JSValue global = getGlobalObject()) {
            global.setProperty(key, value);
        }
    }

    @Override
    public void close() {
        runtime.checkThread();
        cleanable.clean();
        ptr = 0;
    }

    public void checkThread() {
        runtime.checkThread();
    }

    private void checkClosed() {
        if (ptr == 0) {
            throw new IllegalStateException("JSContext is closed");
        }
    }

    private static class NativeContextCleaner implements Runnable {
        private final long ptr;

        NativeContextCleaner(long ptr) {
            this.ptr = ptr;
        }

        @Override
        public void run() {
            freeNativeContext(ptr);
        }
    }

    public JSValue createArray() {
        runtime.checkThread();
        checkClosed();
        long valPtr = createArrayInternal(ptr);
        return new JSValue(valPtr, this);
    }

    public JSValue createObject() {
        runtime.checkThread();
        checkClosed();
        long valPtr = createObjectInternal(ptr);
        return new JSValue(valPtr, this);
    }

    public JSValue toJSValue(Object o) {
        runtime.checkThread();
        checkClosed();
        if (o == null) {
            // How to make undefined or null?
            // We don't have createNull / createUndefined yet.
            // For now, let's assume we want JS null for Java null.
            // Since we lack a direct null factory, we can evaluate "null".
            // Optimization: Add createNullInternal later.
            return eval("null");
        }
        if (o instanceof Integer) {
            return createInteger((Integer) o);
        }
        if (o instanceof String) {
            return createString((String) o);
        }
        if (o instanceof Boolean) {
            // Need createBoolean. For now eval.
            return eval(o.toString());
        }
        if (o instanceof java.util.List) {
            java.util.List<?> list = (java.util.List<?>) o;
            JSValue array = createArray();
            for (int i = 0; i < list.size(); i++) {
                try (JSValue val = toJSValue(list.get(i))) {
                    array.setProperty(i, val);
                }
            }
            return array;
        }
        if (o instanceof java.util.Map) {
            java.util.Map<?, ?> map = (java.util.Map<?, ?>) o;
            JSValue obj = createObject();
            for (java.util.Map.Entry<?, ?> entry : map.entrySet()) {
                String key = entry.getKey().toString();
                try (JSValue val = toJSValue(entry.getValue())) {
                    obj.setProperty(key, val);
                }
            }
            return obj;
        }
        throw new IllegalArgumentException("Unsupported type: " + o.getClass());
    }

    private native long evalInternal(long contextPtr, String script, String fileName, int type);

    private native long parseJSONInternal(long contextPtr, String json);

    private native long createFunctionInternal(long contextPtr, Object callback, String name, int argCount);

    private native long createIntegerInternal(long contextPtr, int value);

    private native long createStringInternal(long contextPtr, String value);

    private native long getGlobalObjectInternal(long contextPtr);

    private native long createArrayInternal(long contextPtr);

    private native long createObjectInternal(long contextPtr);

    private native Long[] createPromiseCapabilityInternal(long contextPtr);

    private native void registerJavaContext(long contextPtr, JSContext thiz);

    private static native void freeNativeContext(long contextPtr);

    public JSValue createPromise(java.util.concurrent.CompletableFuture<?> future) {
        runtime.checkThread();
        checkClosed();

        // [promise, resolve, reject] pointers
        Long[] caps = createPromiseCapabilityInternal(ptr);
        if (caps == null) {
            throw new QuickJSException("Failed to create Promise capability");
        }

        long promisePtr = caps[0];
        long resolvePtr = caps[1];
        long rejectPtr = caps[2];

        JSValue promise = new JSValue(promisePtr, this);
        // We must manage resolve/reject functions. They are JSValues.
        JSValue resolveFunc = new JSValue(resolvePtr, this);
        JSValue rejectFunc = new JSValue(rejectPtr, this);

        future.whenComplete((result, ex) -> {
            // This runs on an arbitrary thread.
            // We must post back to the JS thread.
            runtime.post(() -> {
                try {
                    if (ex != null) {
                        try (JSValue error = createString(ex.getMessage())) { // Simple error for now
                            rejectFunc.call(null, error);
                        }
                    } else {
                        try (JSValue val = toJSValue(result)) {
                            resolveFunc.call(null, val);
                        }
                    }
                } finally {
                    resolveFunc.close();
                    rejectFunc.close();
                }
            });
        });

        return promise;
    }
}
