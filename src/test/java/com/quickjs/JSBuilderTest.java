package com.quickjs;

import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

public class JSBuilderTest {

    @Test
    public void testDefaultCreation() {
        try (JSRuntime runtime = QuickJS.createRuntime()) {
            assertNotNull(runtime);
        }
    }

    @Test
    public void testMemoryLimit() {
        // Use a 2MB limit.
        // This should be enough for basic startup but failing for large allocations.
        try (JSRuntime runtime = QuickJS.builder()
                .withMemoryLimit(2 * 1024 * 1024)
                .build()) {

            try (JSContext context = runtime.createContext()) {
                // Try to allocate 10MB string.
                // This must fail.
                QuickJSException e = assertThrows(QuickJSException.class, () -> {
                    context.eval("var s = 'a'.repeat(10 * 1024 * 1024);");
                });
                // Verify it's an OOM error
                assertTrue(e.getMessage().contains("out of memory") || e instanceof JSInternalError);
            }
        }
    }

    @Test
    public void testMemoryLimitEnforcement() {
        // Create a runtime with 5MB limit
        long limit = 5 * 1024 * 1024;
        try (JSRuntime runtime = QuickJS.builder().withMemoryLimit(limit).build()) {
            try (JSContext context = runtime.createContext()) {
                // Allocate 1MB - should succeed (well within 5MB)
                context.eval("var a = new Uint8Array(1024 * 1024);");

                // Allocate 10MB - should fail (exceeds 5MB)
                assertThrows(QuickJSException.class, () -> {
                    context.eval("var b = new Uint8Array(10 * 1024 * 1024);");
                });
            }
        }
    }
}
