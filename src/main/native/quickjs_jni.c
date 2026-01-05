#include "quickjs.h"
#include <jni.h>
#include <stdlib.h>
#include <string.h>

// Helper to convert jstring to const char*
static const char *GetStringUTFChars(JNIEnv *env, jstring str) {
  if (str == NULL)
    return NULL;
  return (*env)->GetStringUTFChars(env, str, NULL);
}

static void ReleaseStringUTFChars(JNIEnv *env, jstring str, const char *chars) {
  if (str != NULL && chars != NULL) {
    (*env)->ReleaseStringUTFChars(env, str, chars);
  }
}

JNIEXPORT jlong JNICALL
Java_com_quickjs_QuickJS_createNativeRuntime(JNIEnv *env, jclass clazz) {
  JSRuntime *rt = JS_NewRuntime();
  return (jlong)rt;
}

JNIEXPORT void JNICALL Java_com_quickjs_JSRuntime_freeNativeRuntime(
    JNIEnv *env, jobject thiz, jlong runtimePtr) {
  JSRuntime *rt = (JSRuntime *)runtimePtr;
  if (rt) {
    JS_FreeRuntime(rt);
  }
}

JNIEXPORT jlong JNICALL Java_com_quickjs_JSRuntime_createNativeContext(
    JNIEnv *env, jobject thiz, jlong runtimePtr) {
  JSRuntime *rt = (JSRuntime *)runtimePtr;
  if (!rt)
    return 0;
  JSContext *ctx = JS_NewContext(rt);
  return (jlong)ctx;
}

JNIEXPORT void JNICALL Java_com_quickjs_JSContext_freeNativeContext(
    JNIEnv *env, jobject thiz, jlong contextPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  if (ctx) {
    JS_FreeContext(ctx);
  }
}

// Helper to allocate JSValue on heap and return ptr
static jlong boxJSValue(JSValue v) {
  JSValue *p = malloc(sizeof(JSValue));
  *p = v;
  return (jlong)p;
}

JNIEXPORT jlong JNICALL Java_com_quickjs_JSContext_evalInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jstring script) {
  JSContext *ctx = (JSContext *)contextPtr;
  if (!ctx)
    return 0;

  const char *c_script = GetStringUTFChars(env, script);
  if (!c_script)
    return 0;

  JSValue val =
      JS_Eval(ctx, c_script, strlen(c_script), "<input>", JS_EVAL_TYPE_GLOBAL);

  ReleaseStringUTFChars(env, script, c_script);

  if (JS_IsException(val)) {
    JSValue exception_val = JS_GetException(ctx);
    const char *str_res = JS_ToCString(ctx, exception_val);
    JS_FreeValue(ctx, exception_val);
    JS_FreeValue(ctx, val);

    jclass excCls = (*env)->FindClass(env, "com/quickjs/QuickJSException");
    (*env)->ThrowNew(env, excCls, str_res);

    JS_FreeCString(ctx, str_res);
    return 0;
  }

  return boxJSValue(val);
}

JNIEXPORT jint JNICALL Java_com_quickjs_JSValue_getTagInternal(JNIEnv *env,
                                                               jobject thiz,
                                                               jlong contextPtr,
                                                               jlong valPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *v = (JSValue *)valPtr;
  return JS_VALUE_GET_NORM_TAG(*v);
}

JNIEXPORT jint JNICALL Java_com_quickjs_JSValue_toIntegerInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jlong valPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *v = (JSValue *)valPtr;
  int32_t res;
  JS_ToInt32(ctx, &res, *v);
  return res;
}

JNIEXPORT jboolean JNICALL Java_com_quickjs_JSValue_toBooleanInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jlong valPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *v = (JSValue *)valPtr;
  int res = JS_ToBool(ctx, *v);
  if (res == -1) {
    // Exception occurred during conversion
    // TODO: clear and throw? For now just return false.
    JS_FreeValue(ctx, JS_GetException(ctx));
    return 0;
  }
  return (jboolean)res;
}

JNIEXPORT jdouble JNICALL Java_com_quickjs_JSValue_toDoubleInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jlong valPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *v = (JSValue *)valPtr;
  double res;
  if (JS_ToFloat64(ctx, &res, *v) < 0) {
    // Exception
    JS_FreeValue(ctx, JS_GetException(ctx));
    return 0.0; // NaN?
  }
  return res;
}

JNIEXPORT jstring JNICALL Java_com_quickjs_JSValue_toStringInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jlong valPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *v = (JSValue *)valPtr;
  const char *str = JS_ToCString(ctx, *v);
  jstring res = (*env)->NewStringUTF(env, str);
  JS_FreeCString(ctx, str);
  return res;
}

JNIEXPORT void JNICALL Java_com_quickjs_JSValue_closeInternal(JNIEnv *env,
                                                              jobject thiz,
                                                              jlong contextPtr,
                                                              jlong valPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *v = (JSValue *)valPtr;
  JS_FreeValue(ctx, *v);
  free(v);
}
