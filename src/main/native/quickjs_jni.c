#include <jni.h>
#include <string.h>
#include <stdlib.h>
#include "quickjs.h"

// Helper to convert jstring to const char*
static const char* GetStringUTFChars(JNIEnv *env, jstring str) {
    if (str == NULL) return NULL;
    return (*env)->GetStringUTFChars(env, str, NULL);
}

static void ReleaseStringUTFChars(JNIEnv *env, jstring str, const char* chars) {
    if (str != NULL && chars != NULL) {
        (*env)->ReleaseStringUTFChars(env, str, chars);
    }
}

JNIEXPORT jlong JNICALL Java_com_quickjs_QuickJS_createRuntime(JNIEnv *env, jclass clazz) {
    JSRuntime *rt = JS_NewRuntime();
    return (jlong)rt;
}

JNIEXPORT void JNICALL Java_com_quickjs_QuickJS_freeRuntime(JNIEnv *env, jclass clazz, jlong runtimePtr) {
    JSRuntime *rt = (JSRuntime *)runtimePtr;
    if (rt) {
        JS_FreeRuntime(rt);
    }
}

JNIEXPORT jlong JNICALL Java_com_quickjs_QuickJS_createContext(JNIEnv *env, jclass clazz, jlong runtimePtr) {
    JSRuntime *rt = (JSRuntime *)runtimePtr;
    if (!rt) return 0;
    JSContext *ctx = JS_NewContext(rt);
    return (jlong)ctx;
}

JNIEXPORT void JNICALL Java_com_quickjs_QuickJS_freeContext(JNIEnv *env, jclass clazz, jlong contextPtr) {
    JSContext *ctx = (JSContext *)contextPtr;
    if (ctx) {
        JS_FreeContext(ctx);
    }
}

JNIEXPORT jstring JNICALL Java_com_quickjs_QuickJS_evalInternal(JNIEnv *env, jclass clazz, jlong contextPtr, jstring script) {
    JSContext *ctx = (JSContext *)contextPtr;
    if (!ctx) return NULL;

    const char *c_script = GetStringUTFChars(env, script);
    if (!c_script) return NULL;

    // Use "<input>" as filename for now
    JSValue val = JS_Eval(ctx, c_script, strlen(c_script), "<input>", JS_EVAL_TYPE_GLOBAL);

    ReleaseStringUTFChars(env, script, c_script);

    const char *str_res = NULL;
    jstring j_res = NULL;

    if (JS_IsException(val)) {
        JSValue exception_val = JS_GetException(ctx);
        // Ensure we convert exception to string
        str_res = JS_ToCString(ctx, exception_val);
        // We probably want to throw a Java exception here, but for now returned string prefixed?
        // Let's just return the error message for this basic step.
        // A real implementation would map this to a Java exception.
        JS_FreeValue(ctx, exception_val);
    } else {
        str_res = JS_ToCString(ctx, val);
    }
    
    JS_FreeValue(ctx, val);

    if (str_res) {
        j_res = (*env)->NewStringUTF(env, str_res);
        JS_FreeCString(ctx, str_res);
    }

    return j_res;
}
