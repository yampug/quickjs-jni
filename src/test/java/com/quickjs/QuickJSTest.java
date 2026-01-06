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

    @Test
    public void testObjectPropertyAccess() {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {

            try (JSValue obj = context.eval("({})")) {
                try (JSValue val = context.eval("'bar'")) {
                    obj.setProperty("foo", val);
                }

                try (JSValue prop = obj.getProperty("foo")) {
                    assertEquals("bar", prop.asString());
                }
            }
        }
    }

    @Test
    public void testArrayIndexAccess() {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {

            try (JSValue arr = context.eval("['a', 'b']")) {
                try (JSValue item0 = arr.getProperty(0)) {
                    assertEquals("a", item0.asString());
                }

                try (JSValue val = context.eval("'c'")) {
                    arr.setProperty(1, val);
                }

                try (JSValue item1 = arr.getProperty(1)) {
                    assertEquals("c", item1.asString());
                }
            }
        }
    }

    @Test
    public void testFunctionCall() {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {

            try (JSValue func = context.eval("(function(a, b) { return a + b; })")) {
                try (JSValue v1 = context.eval("10");
                        JSValue v2 = context.eval("32")) {
                    try (JSValue result = func.call(null, v1, v2)) {
                        assertEquals(42, result.asInteger());
                    }
                }
            }

            // Test exception
            try (JSValue func = context.eval("(function() { throw new Error('bang'); })")) {
                assertThrows(QuickJSException.class, () -> {
                    func.call(null);
                });
            }
        }
    }
}
