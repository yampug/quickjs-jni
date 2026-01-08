package com.quickjs;

import java.lang.ref.Cleaner;

public class JSValue implements AutoCloseable {
    long ptr;
    private final JSContext context;

    private final Cleaner.Cleanable cleanable;

    JSValue(long ptr, JSContext context) {
        this.ptr = ptr;
        this.context = context;
        this.cleanable = QuickJS.cleaner.register(this, new NativeValueCleaner(ptr, context.ptr));
    }

    public int asInteger() {
        checkThread();
        checkClosed();
        return toIntegerInternal(context.ptr, ptr);
    }

    public boolean asBoolean() {
        checkThread();
        checkClosed();
        return toBooleanInternal(context.ptr, ptr);
    }

    public double asDouble() {
        checkThread();
        checkClosed();
        return toDoubleInternal(context.ptr, ptr);
    }

    public String asString() {
        checkThread();
        checkClosed();
        return toStringInternal(context.ptr, ptr);
    }

    public String toJSON() {
        checkThread();
        checkClosed();
        return toJSONInternal(context.ptr, ptr);
    }

    public int getTypeTag() {
        checkThread();
        checkClosed();
        return getTagInternal(context.ptr, ptr);
    }

    public JSValue getProperty(String key) {
        checkThread();
        checkClosed();
        long resultPtr = getPropertyStrInternal(context.ptr, ptr, key);
        return new JSValue(resultPtr, context);
    }

    public void setProperty(String key, JSValue value) {
        checkThread();
        checkClosed();
        value.checkClosed();
        setPropertyStrInternal(context.ptr, ptr, key, value.ptr);
    }

    public JSValue getProperty(int index) {
        checkThread();
        checkClosed();
        long resultPtr = getPropertyIdxInternal(context.ptr, ptr, index);
        return new JSValue(resultPtr, context);
    }

    public void setProperty(int index, JSValue value) {
        checkThread();
        checkClosed();
        value.checkClosed();
        setPropertyIdxInternal(context.ptr, ptr, index, value.ptr);
    }

    public JSValue call(JSValue thisObj, JSValue... args) {
        checkThread();
        checkClosed();
        long thisPtr = (thisObj != null) ? thisObj.ptr : 0;
        long[] argPtrs = new long[args.length];
        for (int i = 0; i < args.length; i++) {
            args[i].checkClosed();
            argPtrs[i] = args[i].ptr;
        }
        long resultPtr = callInternal(context.ptr, ptr, thisPtr, argPtrs);
        return new JSValue(resultPtr, context);
    }

    @Override
    public void close() {
        checkThread();
        cleanable.clean();
        ptr = 0;
    }

    private void checkClosed() {
        if (ptr == 0) {
            throw new IllegalStateException("JSValue is closed");
        }
    }

    private void checkThread() {
        context.checkThread();
    }

    public int getLength() {
        checkThread();
        checkClosed();
        // Array.length is a property
        try (JSValue lenVal = getProperty("length")) {
            if (lenVal.getTypeTag() == 0) { // JS_TAG_INT (checking tags is raw, usually better methods)
                // But we don't have isUndefined/isNull exposed well yet.
                // Assuming toInteger handles conversions.
            }
            return lenVal.asInteger();
        }
    }

    public String[] getKeys() {
        checkThread();
        checkClosed();
        return getKeysInternal(context.ptr, ptr);
    }

    @SuppressWarnings("unchecked")
    public <T> T toJavaObject(Class<T> clazz) {
        checkThread();
        checkClosed();

        if (clazz == Void.class)
            return null;

        // TODO: Handle NULL and UNDEFINED tags explicitly.
        // Currently relying on type check failure or manual string check in caller.

        if (clazz == Integer.class || clazz == int.class) {
            return (T) Integer.valueOf(asInteger());
        }
        if (clazz == String.class) {
            return (T) asString();
        }
        if (clazz == Boolean.class || clazz == boolean.class) {
            return (T) Boolean.valueOf(asBoolean());
        }
        if (clazz == Double.class || clazz == double.class) {
            return (T) Double.valueOf(asDouble());
        }
        if (java.util.List.class.isAssignableFrom(clazz)) {
            java.util.List<Object> list = new java.util.ArrayList<>();
            int len = getLength();
            for (int i = 0; i < len; i++) {
                try (JSValue item = getProperty(i)) {
                    // Best effort recursion? Or String?
                    // For now, let's map to generic Objects based on tag?
                    // Or just Strings for simplicity?
                    // Let's recurse if we know the target type, but here we have List<?>.
                    // We need dynamic typing.
                    // Simple implementation: String
                    list.add(item.asString());
                }
            }
            return (T) list;
        }

        // TODO: Map support
        return null;
    }

    public JSValue dup() {
        checkThread();
        checkClosed();
        // Native dup needed
        return new JSValue(dupInternal(context.ptr, ptr), context);
    }

    public java.util.concurrent.CompletableFuture<JSValue> toFuture() {
        checkThread();
        checkClosed();

        java.util.concurrent.CompletableFuture<JSValue> future = new java.util.concurrent.CompletableFuture<>();

        try (JSValue thenProp = getProperty("then")) {
            // Check if it's a promise (has 'then' method)
            // Note: simple tag check skipped, assuming caller knows it's a Promise-like
            // object.

            JSFunction onResolve = (ctx, thisObj, args) -> {
                JSValue result = (args.length > 0) ? args[0] : context.eval("undefined");
                future.complete(result.dup());
                return context.createInteger(0);
            };

            JSFunction onReject = (ctx, thisObj, args) -> {
                JSValue err = (args.length > 0) ? args[0] : context.eval("'Unknown Error'");
                future.completeExceptionally(new QuickJSException(err.asString()));
                return context.createInteger(0);
            };

            thenProp.call(this, context.createFunction(onResolve, "resolve", 1),
                    context.createFunction(onReject, "reject", 1));
        }
        return future;
    }

    private static class NativeValueCleaner implements Runnable {
        private final long valPtr;
        private final long ctxPtr;

        NativeValueCleaner(long valPtr, long ctxPtr) {
            this.valPtr = valPtr;
            this.ctxPtr = ctxPtr;
        }

        @Override
        public void run() {
            closeInternal(ctxPtr, valPtr);
        }
    }

    private native int toIntegerInternal(long contextPtr, long valPtr);

    private native boolean toBooleanInternal(long contextPtr, long valPtr);

    private native double toDoubleInternal(long contextPtr, long valPtr);

    private native String toStringInternal(long contextPtr, long valPtr);

    private native String toJSONInternal(long contextPtr, long valPtr);

    private native int getTagInternal(long contextPtr, long valPtr);

    private native long getPropertyStrInternal(long contextPtr, long valPtr, String key);

    private native void setPropertyStrInternal(long contextPtr, long valPtr, String key, long valuePtr);

    private native long getPropertyIdxInternal(long contextPtr, long valPtr, int index);

    private native void setPropertyIdxInternal(long contextPtr, long valPtr, int index, long valuePtr);

    private native long callInternal(long contextPtr, long funcPtr, long thisPtr, long[] args);

    private native String[] getKeysInternal(long contextPtr, long valPtr);

    private static native void closeInternal(long contextPtr, long valPtr);

    private native long dupInternal(long contextPtr, long valPtr);
}
