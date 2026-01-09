package com.quickjs;

import org.junit.jupiter.api.Test;
import static org.junit.jupiter.api.Assertions.*;

public class JSModuleTest {

    @Test
    public void testBasicImport() {
        try (JSRuntime runtime = QuickJS.createRuntime()) {
            runtime.setModuleLoader(moduleName -> {
                if ("foo".equals(moduleName)) {
                    return "export const bar = 42;";
                }
                return null;
            });

            try (JSContext context = runtime.createContext()) {
                JSValue result = context.eval("import { bar } from 'foo'; bar;", "main.js", JSContext.EVAL_TYPE_MODULE);
                // Module evaluation returns a Promise (or undefined if just import?)
                // Actually `import` is a statement.
                // We need to use `import()` expression or top-level await if supported?
                // QuickJS supports top-level await in modules.

                // Wait, `eval` with EVAL_TYPE_MODULE returns the module object or promise?
                // QuickJS docs: JS_Eval with JS_EVAL_TYPE_MODULE returns the module (undefined)
                // or exception.
                // The module is executed asynchronously if it has top level await?
                // We usually need to run the loop.

                // Let's try to export something from main module to check side effects, or use
                // a global.
                context.eval("import { bar } from 'foo'; globalThis.result = bar;", "main.js",
                        JSContext.EVAL_TYPE_MODULE);

                // Modules are async in nature regarding resolution?
                // QuickJS modules are synchronous unless top-level await?
                // Resolving imports happens during eval.

                runtime.runEventLoop();

                assertEquals(42, context.getGlobalObject().getProperty("result").asInteger());
            }
        }
    }

    @Test
    public void testModuleLoaderFailure() {
        try (JSRuntime runtime = QuickJS.createRuntime()) {
            runtime.setModuleLoader(moduleName -> null); // Not found

            try (JSContext context = runtime.createContext()) {
                assertThrows(QuickJSException.class, () -> {
                    context.eval("import { bar } from 'missing';");
                });
            }
        }
    }
}
