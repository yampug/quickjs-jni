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

    public JSValue eval(String script) {
        runtime.checkThread();
        checkClosed();
        long valPtr = evalInternal(ptr, script);
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

    private native long evalInternal(long contextPtr, String script);

    private native long parseJSONInternal(long contextPtr, String json);

    private native long createFunctionInternal(long contextPtr, Object callback, String name, int argCount);

    private native long createIntegerInternal(long contextPtr, int value);

    private native long createStringInternal(long contextPtr, String value);

    private native long getGlobalObjectInternal(long contextPtr);

    private native void registerJavaContext(long contextPtr, JSContext thiz);

    private static native void freeNativeContext(long contextPtr);
}
