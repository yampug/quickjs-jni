package com.quickjs;

public class JSRuntime implements AutoCloseable {
    private long ptr;

    JSRuntime(long ptr) {
        this.ptr = ptr;
    }

    public JSContext createContext() {
        checkClosed();
        long contextPtr = createNativeContext(ptr);
        if (contextPtr == 0) {
            throw new RuntimeException("Failed to create QuickJS context");
        }
        return new JSContext(contextPtr, this);
    }

    @Override
    public void close() {
        if (ptr != 0) {
            freeNativeRuntime(ptr);
            ptr = 0;
        }
    }

    private void checkClosed() {
        if (ptr == 0) {
            throw new IllegalStateException("JSRuntime is closed");
        }
    }

    private native long createNativeContext(long runtimePtr);

    private native void freeNativeRuntime(long runtimePtr);
}
