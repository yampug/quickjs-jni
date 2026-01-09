package com.quickjs;

@FunctionalInterface
public interface JSModuleLoader {
    /**
     * Load the source code of a module.
     * 
     * @param moduleName The name of the module to load (e.g., path or identifier).
     * @return The source code of the module, or null if not found.
     */
    String loadModule(String moduleName);
}
