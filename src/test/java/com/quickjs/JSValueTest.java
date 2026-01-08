package com.quickjs;

import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

public class JSValueTest {

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
}
