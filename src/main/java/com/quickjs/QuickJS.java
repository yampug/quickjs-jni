package com.quickjs;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.lang.ref.Cleaner;

public class QuickJS {

    static final Cleaner cleaner = Cleaner.create();

    static {
        try {
            System.loadLibrary("quickjs-jni");
        } catch (UnsatisfiedLinkError e) {
            try {
                loadNativeLibraryFromJar();
            } catch (IOException ex) {
                throw new RuntimeException("Failed to load native library", ex);
            }
        }
    }

    public static JSRuntime createRuntime() {
        return new Builder().build();
    }

    public static Builder builder() {
        return new Builder();
    }

    public static class Builder {
        private long memoryLimit = -1;
        private long maxStackSize = -1;
        private boolean withStdLib = true;

        public Builder withMemoryLimit(long memoryLimit) {
            this.memoryLimit = memoryLimit;
            return this;
        }

        public Builder withMaxStackSize(long maxStackSize) {
            this.maxStackSize = maxStackSize;
            return this;
        }

        public Builder withoutStdLib() {
            this.withStdLib = false;
            return this;
        }

        public JSRuntime build() {
            long runtimePtr = createNativeRuntime();
            if (runtimePtr == 0) {
                throw new IllegalStateException("Failed to create JSRuntime");
            }
            JSRuntime runtime = new JSRuntime(runtimePtr);
            if (memoryLimit > 0) {
                runtime.setMemoryLimit(memoryLimit);
            }
            if (maxStackSize > 0) {
                runtime.setMaxStackSize(maxStackSize);
            }
            // StdLib is per-context, so we need to store this pref in JSRuntime?
            // Actually, JSRuntime creates the context.
            // So we should pass this config to JSRuntime.
            runtime.setWithStdLib(withStdLib);
            return runtime;
        }
    }

    private static void loadNativeLibraryFromJar() throws IOException {
        String libName = System.mapLibraryName("quickjs-jni");
        try (InputStream is = QuickJS.class.getClassLoader().getResourceAsStream(libName)) {
            if (is == null) {
                throw new IOException("Native library not found in JAR: " + libName);
            }
            File tempLib = File.createTempFile("quickjs-jni", "." + getLibExtension());
            tempLib.deleteOnExit();
            Files.copy(is, tempLib.toPath(), StandardCopyOption.REPLACE_EXISTING);
            System.load(tempLib.getAbsolutePath());
        }
    }

    private static String getLibExtension() {
        String os = System.getProperty("os.name").toLowerCase();
        if (os.contains("win"))
            return "dll";
        if (os.contains("mac"))
            return "dylib";
        return "so";
    }

    private static native long createNativeRuntime();
}
