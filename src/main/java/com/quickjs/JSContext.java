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

    @Override
    public void close() {
        runtime.checkThread();
        // Cleaner.clean() is idempotent
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

    // Native method
    private native long evalInternal(long contextPtr, String script);

    private native long parseJSONInternal(long contextPtr, String json);

    private static native void freeNativeContext(long contextPtr);
}
