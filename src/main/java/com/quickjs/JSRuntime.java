package com.quickjs;

import java.lang.ref.Cleaner;

public class JSRuntime implements AutoCloseable {
    private long ptr;

    private final Thread ownerThread;
    private final Cleaner.Cleanable cleanable;

    JSRuntime(long ptr) {
        this.ptr = ptr;
        this.ownerThread = Thread.currentThread();
        this.cleanable = QuickJS.cleaner.register(this, new NativeRuntimeCleaner(ptr));
    }

    public JSContext createContext() {
        checkThread();
        checkClosed();
        long contextPtr = createNativeContext(ptr);
        if (contextPtr == 0) {
            throw new RuntimeException("Failed to create QuickJS context");
        }
        return new JSContext(contextPtr, this);
    }

    @Override
    public void close() {
        checkThread();
        // Cleaner.clean() is idempotent
        cleanable.clean();
        ptr = 0;
    }

    public void executePendingJob() {
        checkThread();
        checkClosed();
        executePendingJobInternal(ptr);
    }

    public void checkThread() {
        if (Thread.currentThread() != ownerThread) {
            throw new IllegalStateException(
                    "Job execution must be done on the same thread where JSRuntime was created");
        }
    }

    private void checkClosed() {
        if (ptr == 0) {
            throw new IllegalStateException("JSRuntime is closed");
        }
    }

    private static class NativeRuntimeCleaner implements Runnable {
        private final long ptr;

        NativeRuntimeCleaner(long ptr) {
            this.ptr = ptr;
        }

        @Override
        public void run() {
            freeNativeRuntime(ptr);
        }
    }

    private native long createNativeContext(long runtimePtr);

    private native void executePendingJobInternal(long runtimePtr);

    private static native void freeNativeRuntime(long runtimePtr);
}
