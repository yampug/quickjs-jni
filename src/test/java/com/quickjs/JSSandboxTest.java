package com.quickjs;

import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

public class JSSandboxTest {

    @Test
    public void testMaxStackSize() {
        // Limit stack to extremely small size to force overflow quickly.
        // 64KB.
        long stackSize = 64 * 1024;
        try (JSRuntime runtime = QuickJS.builder()
                .withMaxStackSize(stackSize)
                .build()) {
            try (JSContext context = runtime.createContext()) {
                String script = "function recurse(n) { if (n<=0) return; recurse(n-1); } recurse(100000);";

                QuickJSException e = assertThrows(QuickJSException.class, () -> {
                    context.eval(script);
                });

                // Check for standard JS stack overflow error or our custom internal error if
                // applicable
                String msg = e.getMessage();
                boolean isStackOverflow = msg.contains("stack overflow")
                        || msg.contains("Maximum call stack size exceeded");
                assertTrue(isStackOverflow, "Validation failed, got: " + msg);
            }
        }
    }

    @Test
    public void testInfiniteLoopInterruption() {
        try (JSRuntime runtime = QuickJS.builder().build()) {
            try (JSContext context = runtime.createContext()) {

                // Start a thread to interrupt the runtime after a delay
                Thread interrupter = new Thread(() -> {
                    try {
                        Thread.sleep(100); // Wait for script to start
                        runtime.interrupt();
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                });

                interrupter.start();

                double start = System.currentTimeMillis();

                // Infinite loop
                QuickJSException e = assertThrows(QuickJSException.class, () -> {
                    context.eval("while(true) {}");
                });

                assertTrue(e.getMessage().contains("interrupted"));

                // Ensure we didn't run forever
                double duration = System.currentTimeMillis() - start;
                assertTrue(duration < 2000, "Should have been interrupted quickly");

                // Clear interrupt and verify we can run again
                runtime.clearInterrupt();
                assertEquals(42, context.eval("40 + 2").asInteger());
            }
        }
    }

    @Test
    public void testWithoutStdLib() {
        try (JSRuntime runtime = QuickJS.builder().withoutStdLib().build()) {
            try (JSContext context = runtime.createContext()) {
                // Verify Date is undefined (it is separate intrinsic)
                boolean hasDate = context.eval("typeof Date !== 'undefined'").asBoolean();
                assertFalse(hasDate, "Date should not exist in raw context");

                // Verify basic features work
                assertEquals(3, context.eval("1 + 2").asInteger());
            }
        }
    }
}
