package com.quickjs;

import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

public class TypeConversionTest {

    @Test
    public void testTypeConversion() {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {
            // List -> JS Array
            java.util.List<String> list = java.util.Arrays.asList("A", "B", "C");
            try (JSValue arr = context.toJSValue(list)) {
                assertEquals(3, arr.getLength());
                assertEquals("B", arr.getProperty(1).asString());

                // JS Array -> List
                java.util.List<String> back = arr.toJavaObject(java.util.List.class);
                assertEquals(3, back.size());
                assertEquals("C", back.get(2));
            }

            // Map -> JS Object
            java.util.Map<String, Integer> map = new java.util.HashMap<>();
            map.put("x", 100);
            map.put("y", 200);
            try (JSValue obj = context.toJSValue(map)) {
                assertEquals(100, obj.getProperty("x").asInteger());

                String[] keys = obj.getKeys();
                assertEquals(2, keys.length);
            }
        }
    }

    @Test
    public void testNestedTypeConversion() {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {
            // List of Maps
            java.util.List<java.util.Map<String, Integer>> list = new java.util.ArrayList<>();
            java.util.Map<String, Integer> m1 = new java.util.HashMap<>();
            m1.put("a", 1);
            list.add(m1);

            try (JSValue arr = context.toJSValue(list)) {
                assertEquals(1, arr.getLength());
                try (JSValue obj = arr.getProperty(0)) {
                    assertEquals(1, obj.getProperty("a").asInteger());
                }
            }

            // Map of Lists
            java.util.Map<String, java.util.List<String>> map = new java.util.HashMap<>();
            map.put("list", java.util.Arrays.asList("x", "y"));

            try (JSValue obj = context.toJSValue(map)) {
                try (JSValue arr = obj.getProperty("list")) {
                    assertEquals(2, arr.getLength());
                    assertEquals("x", arr.getProperty(0).asString());
                }
            }
        }
    }

    @Test
    public void testEdgeCases() {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {
            // Empty List
            try (JSValue arr = context.toJSValue(java.util.Collections.emptyList())) {
                assertEquals(0, arr.getLength());
                java.util.List<Object> back = arr.toJavaObject(java.util.List.class);
                assertTrue(back.isEmpty());
            }

            // Empty Map
            try (JSValue obj = context.toJSValue(java.util.Collections.emptyMap())) {
                assertEquals(0, obj.getKeys().length);
            }

            // Null
            try (JSValue val = context.toJSValue(null)) {
                // We don't have isNull() exposed yet, but we can verify it's safely created
                // and maybe check tag if we exposed it, or try to eval against it
            }
        }
    }

    @Test
    public void testUnsupportedTypes() {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {
            assertThrows(IllegalArgumentException.class, () -> {
                context.toJSValue(new Object());
            });
        }
    }
}
