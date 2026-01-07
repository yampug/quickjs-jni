package com.quickjs;

@FunctionalInterface
public interface JSFunction {
    JSValue apply(JSContext context, JSValue thisObj, JSValue[] args);
}
