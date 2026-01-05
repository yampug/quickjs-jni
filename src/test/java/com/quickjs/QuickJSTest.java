package com.quickjs;

import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

public class QuickJSTest {
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
    public void testPrimitiveConversions() {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {

            try (JSValue v1 = context.eval("true")) {
                assertTrue(v1.asBoolean());
            }

            try (JSValue v2 = context.eval("false")) {
                assertFalse(v2.asBoolean());
            }

            try (JSValue v3 = context.eval("3.14")) {
                assertEquals(3.14, v3.asDouble(), 0.0001);
            }

            try (JSValue v4 = context.eval("'hello'")) {
                assertEquals("hello", v4.asString());
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
}
