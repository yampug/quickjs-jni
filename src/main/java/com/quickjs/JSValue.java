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

    public int getTypeTag() {
        checkClosed();
        return getTagInternal(context.ptr, ptr);
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

    private native int getTagInternal(long contextPtr, long valPtr);

    private native void closeInternal(long contextPtr, long valPtr);
}
