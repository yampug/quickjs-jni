package com.quickjs;

public class JSContext implements AutoCloseable {
    long ptr;
    private final JSRuntime runtime;

    JSContext(long ptr, JSRuntime runtime) {
        this.ptr = ptr;
        this.runtime = runtime;
    }

    public JSValue eval(String script) {
        checkClosed();
        long valPtr = evalInternal(ptr, script);
        return new JSValue(valPtr, this);
    }

    public JSValue parseJSON(String json) {
        checkClosed();
        long valPtr = parseJSONInternal(ptr, json);
        return new JSValue(valPtr, this);
    }

    @Override
    public void close() {
        if (ptr != 0) {
            freeNativeContext(ptr);
            ptr = 0;
        }
    }

    private void checkClosed() {
        if (ptr == 0) {
            throw new IllegalStateException("JSContext is closed");
        }
    }

    // Native method
    private native long evalInternal(long contextPtr, String script);

    private native long parseJSONInternal(long contextPtr, String json);

    private native void freeNativeContext(long contextPtr);
}
