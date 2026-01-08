package com.quickjs;

import org.junit.jupiter.api.Test;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

import static org.junit.jupiter.api.Assertions.*;

public class JSPromiseTest {

    @Test
    public void testJavaToJSPromiseResolve() throws Exception {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {

            CompletableFuture<String> javaFuture = new CompletableFuture<>();

            // Create a JS Promise from the Java Future
            JSValue jsPromise = context.createPromise(javaFuture);
            context.setGlobal("myPromise", jsPromise);

            // Attach a JS handler
            context.eval("var result = null; myPromise.then(function(val) { result = val; });");

            // Verify not resolved yet
            String initialVal = context.getGlobalObject().getProperty("result").toJavaObject(String.class);
            // JS null converts to "null" string in current implementation
            assertTrue("null".equals(initialVal) || initialVal == null);

            // Complete safely
            javaFuture.complete("Success");

            // Run loop
            runtime.runEventLoop();

            // Verify
            assertEquals("Success", context.eval("result").asString());
        }
    }

    @Test
    public void testJavaToJSPromiseReject() throws Exception {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {

            CompletableFuture<String> javaFuture = new CompletableFuture<>();
            JSValue jsPromise = context.createPromise(javaFuture);
            context.setGlobal("myPromise", jsPromise);

            context.eval("var error = null; myPromise.catch(function(err) { error = err; });");

            javaFuture.completeExceptionally(new RuntimeException("Failure"));

            runtime.runEventLoop();

            assertTrue(context.eval("error").asString().contains("Failure"));
        }
    }

    @Test
    public void testJSToJavaPromise() throws Exception {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {

            // JS Promise that resolves immediately
            JSValue jsPromise = context.eval("Promise.resolve('JS Rules')");

            CompletableFuture<JSValue> future = jsPromise.toFuture();

            // Run loop to process microtasks
            runtime.runEventLoop();

            // Future should be done
            assertTrue(future.isDone());
            assertEquals("JS Rules", future.get().asString());
        }
    }

    @Test
    public void testJSToJavaPromiseAsync() throws Exception {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {

            // Simple JS timer or something? We don't have setTimeout unless we implement
            // it.
            // But we can trigger resolution manually via another promise capability or
            // function.

            context.eval("var resolveFunc; var p = new Promise(function(r) { resolveFunc = r; });");
            JSValue p = context.eval("p");

            CompletableFuture<JSValue> future = p.toFuture();

            assertFalse(future.isDone());

            context.eval("resolveFunc('Async Done')");
            runtime.runEventLoop();

            assertTrue(future.isDone());
            assertEquals("Async Done", future.get().asString());
        }
    }

    @Test
    public void testThreadSafety() throws Exception {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {

            CompletableFuture<String> future = new CompletableFuture<>();
            JSValue jsPromise = context.createPromise(future);
            context.setGlobal("p", jsPromise);
            context.eval("var res; p.then(v => res = v);");

            // Complete on background thread
            new Thread(() -> {
                try {
                    Thread.sleep(50);
                } catch (InterruptedException e) {
                }
                future.complete("Thread-Safe");
            }).start();

            // Pump event loop for a bit
            long start = System.currentTimeMillis();
            while (System.currentTimeMillis() - start < 500) {
                runtime.runEventLoop();
                Thread.sleep(10);
                JSValue val = context.eval("res");
                if (val.getTypeTag() != 0 && val.getTypeTag() != 7) { // Wait for result (not undefined/null)
                    if ("Thread-Safe".equals(val.asString())) {
                        break;
                    }
                }
                val.close();
            }

            assertEquals("Thread-Safe", context.eval("res").asString());
        }
    }
}
