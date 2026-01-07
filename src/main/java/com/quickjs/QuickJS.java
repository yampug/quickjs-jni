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
        long runtimePtr = createNativeRuntime();
        if (runtimePtr == 0) {
            throw new IllegalStateException("Failed to create JSRuntime");
        }
        return new JSRuntime(runtimePtr);
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
