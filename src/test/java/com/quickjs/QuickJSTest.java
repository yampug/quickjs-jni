package com.quickjs;

import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

public class QuickJSTest {
    @Test
    public void testEval() {
        try (QuickJS qjs = QuickJS.create()) {
            String result = qjs.eval("1 + 2");
            assertEquals("3", result);
        }
    }

    @Test
    public void testException() {
        try (QuickJS qjs = QuickJS.create()) {
            // This should return the error string for now
            String result = qjs.eval("throw new Error('foo')");
            // Our current impl catches exception and returns the string
            assertTrue(result.contains("Error: foo"));
        }
    }
}
