package com.quickjs;

public class QuickJS {
    static final java.lang.ref.Cleaner cleaner = java.lang.ref.Cleaner.create();
    private static boolean libraryLoaded = false;

    static {
        try {
            System.loadLibrary("quickjs-jni");
            libraryLoaded = true;
        } catch (UnsatisfiedLinkError e) {
            e.printStackTrace();
        }
    }

    public static JSRuntime createRuntime() {
        if (!libraryLoaded) {
            throw new IllegalStateException("QuickJS JNI library not loaded");
        }
        long runtimePtr = createNativeRuntime();
        if (runtimePtr == 0) {
            throw new RuntimeException("Failed to create QuickJS runtime");
        }
        return new JSRuntime(runtimePtr);
    }

    private static native long createNativeRuntime();
}
