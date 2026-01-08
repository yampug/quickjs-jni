package com.quickjs;

import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

public class JSExceptionTest {

    @Test
    public void testSyntaxError() {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {

            JSSyntaxError e = assertThrows(JSSyntaxError.class, () -> {
                context.eval("var a = ;");
            });
            assertTrue(e.getMessage().contains("unexpected token"), "Message should contain detail");
        }
    }

    @Test
    public void testReferenceError() {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {

            JSReferenceError e = assertThrows(JSReferenceError.class, () -> {
                context.eval("unknownVar;");
            });
            assertTrue(e.getMessage().contains("is not defined"), "Message should contain detail");
        }
    }

    @Test
    public void testThrowString() {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {

            QuickJSException e = assertThrows(QuickJSException.class, () -> {
                context.eval("throw 'My Error';");
            });
            assertTrue(e.getMessage().contains("My Error"));
        }
    }

    @Test
    public void testStackTrace() {
        try (JSRuntime runtime = QuickJS.createRuntime();
                JSContext context = runtime.createContext()) {

            QuickJSException e = assertThrows(QuickJSException.class, () -> {
                context.eval("function foo() { throw new Error('Inside foo'); } function bar() { foo(); } bar();");
            });
            String msg = e.getMessage();
            assertTrue(msg.contains("Inside foo"));
            assertTrue(msg.contains("at foo"), "Should contain stack trace 'at foo'");
            assertTrue(msg.contains("at bar"), "Should contain stack trace 'at bar'");
        }
    }
}
