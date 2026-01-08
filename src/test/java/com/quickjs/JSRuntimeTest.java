package com.quickjs;

import org.junit.jupiter.api.Test;
import java.util.concurrent.atomic.AtomicBoolean;
import static org.junit.jupiter.api.Assertions.*;

public class JSRuntimeTest {

    @Test
    public void testEval() {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {
            try (JSValue result = context.eval("1 + 2")) {
                assertEquals(3, result.asInteger());
            }
        }
    }

    @Test
    public void testException() {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {
            QuickJSException exception = assertThrows(QuickJSException.class, () -> {
                context.eval("throw new Error('foo')");
            });
            assertTrue(exception.getMessage().contains("Error: foo"));
        }
    }

    @Test
    public void testThreadSafety() throws InterruptedException {
        try (JSRuntime runtime = QuickJS.createRuntime()) {
            AtomicBoolean threadFailed = new AtomicBoolean(false);
            Thread t = new Thread(() -> {
                try {
                    runtime.createContext();
                } catch (IllegalStateException e) {
                    if (e.getMessage().contains("thread")) {
                        threadFailed.set(true);
                    }
                }
            });
            t.start();
            t.join();
            assertTrue(threadFailed.get(), "Should fail when accessing JSRuntime from another thread");
        }
    }
}
