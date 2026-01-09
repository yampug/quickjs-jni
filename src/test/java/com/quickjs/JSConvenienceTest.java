package com.quickjs;

import org.junit.jupiter.api.Test;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;

import static org.junit.jupiter.api.Assertions.*;

public class JSConvenienceTest {

    @Test
    public void testIterable() {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {

            try (JSValue array = context.createArray()) {
                array.setProperty(0, context.createInteger(10));
                array.setProperty(1, context.createInteger(20));
                array.setProperty(2, context.createInteger(30));

                List<Integer> values = new ArrayList<>();
                for (JSValue val : array) {
                    values.add(val.asInteger());
                    val.close(); // Iterator returns new JSValue, we should close it inside loop?
                    // Usage pattern: `for (JSValue v : array) { try(v) { ... } }` or just rely on
                    // GC/Cleaner?
                    // Best practice: use try-with-resources if possible, but foreach loop hides
                    // iterator.
                    // The iterator.next() returns a new JSValue.
                }

                // If we didn't close, cleaner would handle it, but explicit close is better.
                // Re-verify the loop logic:
                // `for (JSValue val : array)` -> val is available in loop.
                // We should probably explicitly close `val` at end of loop body if we want
                // determinism.

                assertEquals(3, values.size());
                assertEquals(10, values.get(0));
                assertEquals(20, values.get(1));
                assertEquals(30, values.get(2));
            }
        }
    }

    @Test
    public void testHas() {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {

            try (JSValue obj = context.createObject()) {
                obj.setProperty("foo", context.createInteger(123));

                assertTrue(obj.has("foo"));
                assertFalse(obj.has("bar"));
            }
        }
    }

    @Test
    public void testInvokeMember() {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {

            context.eval("var Math = { add: function(a, b) { return a + b; } };");
            try (JSValue math = context.getGlobalObject().getProperty("Math")) {
                try (JSValue result = math.invokeMember("add", 10, 20)) {
                    assertEquals(30, result.asInteger());
                }
            }
        }
    }

    @Test
    public void testEvalPath() throws java.io.IOException {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {

            Path tempFile = Files.createTempFile("test_script", ".js");
            Files.writeString(tempFile, "var fromFile = 999;");

            try {
                context.eval(tempFile);
                assertEquals(999, context.getGlobalObject().getProperty("fromFile").asInteger());
            } finally {
                Files.delete(tempFile);
            }
        }
    }
}
