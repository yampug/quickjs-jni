package com.quickjs;

import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

public class JSFunctionTest {

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

    @Test
    public void testCallbacks() {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {
            try (JSValue addFunc = context.createFunction((ctx, thisObj, args) -> {
                int a = args[0].asInteger();
                int b = args[1].asInteger();
                return context.createInteger(a + b);
            }, "add", 2)) {
                context.setGlobal("add", addFunc);

                try (JSValue resultVal = context.eval("add(10, 20)")) {
                    int result = resultVal.asInteger();
                    assertEquals(30, result);
                }
            }
        }
    }
}
