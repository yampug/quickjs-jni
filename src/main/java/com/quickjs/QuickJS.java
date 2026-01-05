package com.quickjs;

public class QuickJS implements AutoCloseable {
    private long runtimePtr;
    private long contextPtr;
    private static boolean libraryLoaded = false;

    static {
        try {
            System.loadLibrary("quickjs-jni");
            libraryLoaded = true;
        } catch (UnsatisfiedLinkError e) {
            // Can be loaded manually or handled by wrapper
            e.printStackTrace();
        }
    }

    private QuickJS(long runtimePtr, long contextPtr) {
        this.runtimePtr = runtimePtr;
        this.contextPtr = contextPtr;
    }

    public static QuickJS create() {
        if (!libraryLoaded) {
            throw new IllegalStateException("QuickJS JNI library not loaded");
        }
        long runtime = createRuntime();
        if (runtime == 0) {
            throw new RuntimeException("Failed to create QuickJS runtime");
        }
        long context = createContext(runtime);
        if (context == 0) {
            freeRuntime(runtime);
            throw new RuntimeException("Failed to create QuickJS context");
        }
        return new QuickJS(runtime, context);
    }

    public String eval(String _script) {
        checkClosed();
        return evalInternal(contextPtr, _script);
    }

    @Override
    public void close() {
        if (contextPtr != 0) {
            freeContext(contextPtr);
            contextPtr = 0;
        }
        if (runtimePtr != 0) {
            freeRuntime(runtimePtr);
            runtimePtr = 0;
        }
    }

    private void checkClosed() {
        if (contextPtr == 0 || runtimePtr == 0) {
            throw new IllegalStateException("QuickJS instance is closed");
        }
    }

    // Native methods
    private static native long createRuntime();

    private static native void freeRuntime(long runtime);

    private static native long createContext(long runtime);

    private static native void freeContext(long context);

    private static native String evalInternal(long context, String script);
}
