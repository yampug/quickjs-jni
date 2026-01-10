package com.quickjs;

import java.lang.ref.Cleaner;

public class JSRuntime implements AutoCloseable {
    private long ptr;
    private final Thread ownerThread;
    private final Cleaner.Cleanable cleanable;
    private final java.util.Queue<Runnable> jobQueue = new java.util.concurrent.ConcurrentLinkedQueue<>();
    private volatile boolean closed = false;

    JSRuntime(long ptr) {
        this.ptr = ptr;
        this.ownerThread = Thread.currentThread();
        this.cleanable = QuickJS.cleaner.register(this, new NativeRuntimeCleaner(ptr));
    }

    public static JSRuntime create() {
        return new JSRuntime(createRuntimeInternal());
    }

    public long getPtr() {
        checkThread();
        return ptr;
    }

    public JSContext createContext() {
        checkThread();
        checkClosed();
        long contextPtr = createNativeContext(ptr, withStdLib);
        if (contextPtr == 0) {
            throw new RuntimeException("Failed to create QuickJS context");
        }
        return new JSContext(contextPtr, this);
    }

    public void checkThread() {
        if (Thread.currentThread() != ownerThread) {
            throw new IllegalStateException("JSRuntime used on wrong thread. Access is single-threaded.");
        }
    }

    private void checkClosed() {
        if (closed) {
            throw new IllegalStateException("JSRuntime is closed");
        }
    }

    /**
     * Post a runnable to be executed on the JSRuntime thread during the next call
     * to {@link #runEventLoop()}.
     * This method is thread-safe and can be called from any thread.
     */
    public void post(Runnable job) {
        jobQueue.add(job);
    }

    /**
     * Execute all pending Java jobs and QuickJS microtasks.
     * This must be called periodically to process Promises and callbacks from other
     * threads.
     */
    public void runEventLoop() {
        checkThread();
        checkClosed();

        // 1. Process Java jobs (e.g. CompletableFuture callbacks)
        Runnable job;
        while ((job = jobQueue.poll()) != null) {
            try {
                job.run();
            } catch (Throwable t) {
                t.printStackTrace(); // or log, don't crash the loop
            }
        }

        // 2. Process QuickJS pending jobs (Microtasks/Promises)
        while (executePendingJobInternal(ptr))
            ;
    }

    @Override
    public void close() {
        if (!closed) {
            closed = true;
            // Native cleaner will handle freeRuntimeInternal(ptr);
        }
    }

    private static class NativeRuntimeCleaner implements Runnable {
        private final long ptr;

        NativeRuntimeCleaner(long ptr) {
            this.ptr = ptr;
        }

        @Override
        public void run() {
            freeRuntimeInternal(ptr);
        }
    }

    public void setModuleLoader(JSModuleLoader loader) {
        checkThread();
        checkClosed();
        // Keep a strong reference to the loader to prevent GC if we only hold weak ref
        // in native (or if native holds global ref)
        // Native side will hold a GlobalRef.
        setModuleLoaderInternal(ptr, loader);
    }

    public void setMemoryLimit(long limit) {
        checkThread();
        checkClosed();
        setMemoryLimitInternal(ptr, limit);
    }

    public void setMaxStackSize(long size) {
        checkThread();
        checkClosed();
        setMaxStackSizeInternal(ptr, size);
    }

    public void interrupt() {
        // Can be called from any thread?
        // Interrupt logic usually involves setting a flag checked by another thread.
        // QuickJS's JS_SetInterruptHandler callback is run on the JS thread.
        // We need to set a flag that the callback reads.
        // Thread safety: This method MUST be thread-safe as it's intended to stop
        // a runaway script on the JS thread.
        setInterruptInternal(ptr);
    }

    public void clearInterrupt() {
        // To allow resuming usage of the runtime after an interrupt.
        // Not strictly standard but often needed.
        clearInterruptInternal(ptr);
    }

    // Internal config
    private boolean withStdLib = true;

    void setWithStdLib(boolean withStdLib) {
        this.withStdLib = withStdLib;
    }

    private native void setMemoryLimitInternal(long runtimePtr, long limit);

    private native void setMaxStackSizeInternal(long runtimePtr, long size);

    private native void setInterruptInternal(long runtimePtr);

    private native void clearInterruptInternal(long runtimePtr);

    private native void setModuleLoaderInternal(long runtimePtr, JSModuleLoader loader);

    private static native long createRuntimeInternal();

    private static native void freeRuntimeInternal(long ptr);

    private native long createNativeContext(long runtimePtr, boolean withStdLib);

    private static native boolean executePendingJobInternal(long ptr);
}
