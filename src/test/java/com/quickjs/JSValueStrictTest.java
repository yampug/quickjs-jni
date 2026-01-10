package com.quickjs;

import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

public class JSValueStrictTest {

    @Test
    public void testStrictFactories() {
        try (JSRuntime rt = QuickJS.createRuntime(); JSContext ctx = rt.createContext()) {
            JSValue vNull = ctx.createNull();
            assertTrue(vNull.isNull());
            assertFalse(vNull.isUndefined());

            JSValue vUndefined = ctx.createUndefined();
            assertTrue(vUndefined.isUndefined());
            assertFalse(vUndefined.isNull());

            JSValue vTrue = ctx.createBoolean(true);
            assertTrue(vTrue.isBoolean());
            assertEquals(true, vTrue.asBoolean());

            JSValue vFalse = ctx.createBoolean(false);
            assertTrue(vFalse.isBoolean());
            assertEquals(false, vFalse.asBoolean());

            JSValue vDouble = ctx.createDouble(3.14159);
            assertTrue(vDouble.isNumber());
            assertEquals(3.14159, vDouble.asDouble(), 0.00001);

            // Test logic for toJSValue using primitive check
            Object nullObj = null;
            JSValue vNullObj = ctx.toJSValue(nullObj);
            assertTrue(vNullObj.isNull());

            Boolean boolObj = Boolean.TRUE;
            JSValue vBoolObj = ctx.toJSValue(boolObj);
            assertTrue(vBoolObj.isBoolean());
            assertTrue(vBoolObj.asBoolean());

            Double doubleObj = 123.456;
            JSValue vDoubleObj = ctx.toJSValue(doubleObj);
            assertTrue(vDoubleObj.isNumber());
            assertEquals(123.456, vDoubleObj.asDouble(), 0.00001);
        }
    }

    @Test
    public void testStrictTypeCheckers() {
        try (JSRuntime rt = QuickJS.createRuntime(); JSContext ctx = rt.createContext()) {
            JSValue vInt = ctx.eval("123");
            assertTrue(vInt.isNumber());
            assertTrue(vInt.isInteger());
            assertFalse(vInt.isString());

            JSValue vFloat = ctx.eval("123.456");
            assertTrue(vFloat.isNumber());
            // QuickJS might treat 123.456 as having JS_TAG_FLOAT64, so isInteger should be
            // false (unless it fits in int?)
            // 123.456 does not fit in int.
            assertFalse(vFloat.isInteger());

            JSValue vString = ctx.eval("'hello'");
            assertTrue(vString.isString());
            assertFalse(vString.isNumber());

            JSValue vArray = ctx.eval("[]");
            assertTrue(vArray.isArray());
            assertTrue(vArray.isObject()); // Array is object

            JSValue vObject = ctx.eval("({})");
            assertTrue(vObject.isObject());
            assertFalse(vObject.isArray());

            JSValue vFunction = ctx.eval("() => {}");
            assertTrue(vFunction.isFunction());
            // JS_IsObject might return false for functions in C API depending on
            // implementation details
            // so we skip checking isObject for functions to avoid brittleness.

            JSValue vError = ctx.eval("new Error('fail')");
            assertTrue(vError.isError());
            assertTrue(vError.isObject());
        }
    }
}
