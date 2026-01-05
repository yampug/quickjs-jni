cmake_minimum_required(VERSION 3.10)
project(quickjs-jni)

# Find JNI
find_package(JNI REQUIRED)

# Include QuickJS headers (assuming we are in bindings/java/src/main/native)
# Root is at ../../../../../
set(QUICKJS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")

include_directories(${JNI_INCLUDE_DIRS})
include_directories(${QUICKJS_ROOT})

# Add source files
file(GLOB QUICKJS_SOURCES "${QUICKJS_ROOT}/*.c")

# Exclude main files and utilities not needed for the library
list(FILTER QUICKJS_SOURCES EXCLUDE REGEX ".*/qjs\\.c$")
list(FILTER QUICKJS_SOURCES EXCLUDE REGEX ".*/qjsc\\.c$")
list(FILTER QUICKJS_SOURCES EXCLUDE REGEX ".*/unicode_gen\\.c$")
list(FILTER QUICKJS_SOURCES EXCLUDE REGEX ".*/.*test.*\\.c$")
list(FILTER QUICKJS_SOURCES EXCLUDE REGEX ".*/fuzz\\.c$")
# list(FILTER QUICKJS_SOURCES EXCLUDE REGEX ".*/dtoa\\.c$") # dtoa is needed! 



add_library(quickjs-jni SHARED quickjs_jni.c ${QUICKJS_SOURCES})

# Need -lm and -ldl usually for QuickJS
if(UNIX)
    target_link_libraries(quickjs-jni m dl)
endif()

# JNI libs
target_link_libraries(quickjs-jni ${JNI_LIBRARIES})
