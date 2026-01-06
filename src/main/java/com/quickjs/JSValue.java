package com.quickjs;

public class JSValue implements AutoCloseable {
    long ptr; // Pointer to JSValue on native heap
    private final JSContext context;

    JSValue(long ptr, JSContext context) {
        this.ptr = ptr;
        this.context = context;
    }

    public int asInteger() {
        checkClosed();
        // Access context.ptr - assumes package private
        return toIntegerInternal(context.ptr, ptr);
    }

    public boolean asBoolean() {
        checkClosed();
        return toBooleanInternal(context.ptr, ptr);
    }

    public double asDouble() {
        checkClosed();
        return toDoubleInternal(context.ptr, ptr);
    }

    public String asString() {
        checkClosed();
        return toStringInternal(context.ptr, ptr);
    }

    public String toJSON() {
        checkClosed();
        return toJSONInternal(context.ptr, ptr);
    }

    public int getTypeTag() {
        checkClosed();
        return getTagInternal(context.ptr, ptr);
    }

    public JSValue getProperty(String key) {
        checkClosed();
        long resultPtr = getPropertyStrInternal(context.ptr, ptr, key);
        return new JSValue(resultPtr, context);
    }

    public void setProperty(String key, JSValue value) {
        checkClosed();
        value.checkClosed(); // Ensure value is also valid
        setPropertyStrInternal(context.ptr, ptr, key, value.ptr);
    }

    public JSValue getProperty(int index) {
        checkClosed();
        long resultPtr = getPropertyIdxInternal(context.ptr, ptr, index);
        return new JSValue(resultPtr, context);
    }

    public void setProperty(int index, JSValue value) {
        checkClosed();
        value.checkClosed();
        setPropertyIdxInternal(context.ptr, ptr, index, value.ptr);
    }

    public JSValue call(JSValue thisObj, JSValue... args) {
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
        if (ptr != 0) {
            closeInternal(context.ptr, ptr);
            ptr = 0;
        }
    }

    private void checkClosed() {
        if (ptr == 0) {
            throw new IllegalStateException("JSValue is closed");
        }
    }

    // Native lookups
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

    private native void closeInternal(long contextPtr, long valPtr);
}
