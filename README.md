# QuickJS-JNI

This library provides Java bindings for the [QuickJS](https://bellard.org/quickjs/) JavaScript engine.
It allows you to embed a JavaScript runtime in any Java 11+ application. The library has zero external dependencies so no need to worry about version conflicts!

[![License](https://img.shields.io/badge/license-MIT-green?labelColor=gray)](LICENSE.md)

## Installation

Assuming you have built the native library and JAR, include them in your project.

TODO build instructions

## Basic Usage

### 1. Create a Runtime and Context

QuickJS requires a `JSRuntime` and `JSContext`. Always use `try-with-resources` to ensure native resources are freed.

```java
try (JSRuntime runtime = QuickJS.createRuntime()) {
    try (JSContext context = runtime.createContext()) {
        // Your code here
    }
}
```

### 2. Evaluating JavaScript

You can evaluate any JavaScript code.

```java
int result = context.eval("1 + 2").asInteger();
System.out.println(result); // 3

String greeting = context.eval("'Hello ' + 'World'").asString();
System.out.println(greeting); // Hello World
```

### 3. Working with Objects

You can access and modify JavaScript objects.

```java
JSValue obj = context.eval("({ name: 'Alice', age: 30 })");
String name = obj.getProperty("name").asString();
obj.setProperty("age", context.eval("31"));
```

### 4. Working with Arrays

```java
JSValue arr = context.eval("['apple', 'banana']");
String fruit = arr.getProperty(0).asString(); // apple
arr.setProperty(1, context.eval("'cherry'"));
```

### 5. Calling Functions

You can define functions in JS and call them from Java.

```java
context.eval("function add(a, b) { return a + b; }");
JSValue addFunc = context.eval("add");
int sum = addFunc.call(null, context.eval("10"), context.eval("20")).asInteger(); // 30
```

### 6. JSON Support

```java
JSValue data = context.parseJSON("{\"foo\": \"bar\"}");
String json = data.toJSON();
```

### 7. Promises and Async/Await

QuickJS uses a job queue for Promises. You must manually execute pending jobs.

```java
context.eval("Promise.resolve('done').then(console.log)");
runtime.executePendingJob(); // Executes the 'then' callback
```

## Important Considerations

### Thread Safety
**QuickJS is NOT thread-safe.**
A `JSRuntime` and all its `JSContext` and `JSValue` objects must be accessed from the **same thread** where the `JSRuntime` was created. Accessing them from another thread will throw an `IllegalStateException`.

### Resource Management
Native memory is managed manually. While we use `Cleaner` as a safety net, you should **always** explicitly close resources using `try-with-resources` or `.close()` to avoid memory pressure.

```java
// Good practice
try (JSValue val = context.eval("...")) {
    // use val
} // val.close() called automatically
```
