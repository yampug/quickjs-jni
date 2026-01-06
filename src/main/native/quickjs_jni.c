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

JNIEXPORT void JNICALL Java_com_quickjs_JSRuntime_executePendingJobInternal(
    JNIEnv *env, jobject thiz, jlong runtimePtr) {
  JSRuntime *rt = (JSRuntime *)runtimePtr;
  if (!rt)
    return;

  JSContext *ctx;
  while (1) {
    int err = JS_ExecutePendingJob(rt, &ctx);
    if (err <= 0) {
      if (err < 0) {
        // Exception
        if (ctx) {
          JSValue exception_val = JS_GetException(ctx);
          const char *str_res = JS_ToCString(ctx, exception_val);
          JS_FreeValue(ctx, exception_val);

          jclass excCls =
              (*env)->FindClass(env, "com/quickjs/QuickJSException");
          (*env)->ThrowNew(env, excCls, str_res);

          JS_FreeCString(ctx, str_res);
        } else {
          jclass excCls =
              (*env)->FindClass(env, "com/quickjs/QuickJSException");
          (*env)->ThrowNew(env, excCls, "Unknown error during job execution");
        }
      }
      break;
    }
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

JNIEXPORT jlong JNICALL Java_com_quickjs_JSValue_getPropertyStrInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jlong valPtr, jstring key) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *obj = (JSValue *)valPtr;

  const char *c_key = GetStringUTFChars(env, key);
  if (!c_key)
    return 0;

  JSValue result = JS_GetPropertyStr(ctx, *obj, c_key);

  ReleaseStringUTFChars(env, key, c_key);

  return boxJSValue(result);
}

JNIEXPORT void JNICALL Java_com_quickjs_JSValue_setPropertyStrInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jlong valPtr, jstring key,
    jlong valuePtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *obj = (JSValue *)valPtr;
  JSValue *val = (JSValue *)valuePtr;

  const char *c_key = GetStringUTFChars(env, key);
  if (!c_key)
    return;

  // JS_SetPropertyStr takes ownership of the value.
  // Since the Java JSValue object still owns 'val', we must duplicate it.
  JSValue val_dup = JS_DupValue(ctx, *val);

  int res = JS_SetPropertyStr(ctx, *obj, c_key, val_dup);

  ReleaseStringUTFChars(env, key, c_key);

  if (res == -1) {
    JSValue exception_val = JS_GetException(ctx);
    const char *str_res = JS_ToCString(ctx, exception_val);
    JS_FreeValue(ctx, exception_val);

    jclass excCls = (*env)->FindClass(env, "com/quickjs/QuickJSException");
    (*env)->ThrowNew(env, excCls, str_res);

    JS_FreeCString(ctx, str_res);
  }
}

JNIEXPORT jlong JNICALL Java_com_quickjs_JSValue_getPropertyIdxInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jlong valPtr, jint index) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *obj = (JSValue *)valPtr;

  JSValue result = JS_GetPropertyUint32(ctx, *obj, (uint32_t)index);

  return boxJSValue(result);
}

JNIEXPORT void JNICALL Java_com_quickjs_JSValue_setPropertyIdxInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jlong valPtr, jint index,
    jlong valuePtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *obj = (JSValue *)valPtr;
  JSValue *val = (JSValue *)valuePtr;

  JSValue val_dup = JS_DupValue(ctx, *val);

  int res = JS_SetPropertyUint32(ctx, *obj, (uint32_t)index, val_dup);

  if (res == -1) {
    JSValue exception_val = JS_GetException(ctx);
    const char *str_res = JS_ToCString(ctx, exception_val);
    JS_FreeValue(ctx, exception_val);

    jclass excCls = (*env)->FindClass(env, "com/quickjs/QuickJSException");
    (*env)->ThrowNew(env, excCls, str_res);

    JS_FreeCString(ctx, str_res);
  }
}

JNIEXPORT jlong JNICALL Java_com_quickjs_JSValue_callInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jlong funcPtr, jlong thisPtr,
    jlongArray args) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *func = (JSValue *)funcPtr;
  JSValue this_val;
  if (thisPtr == 0) {
    this_val = JS_UNDEFINED;
  } else {
    this_val = *(JSValue *)thisPtr;
  }

  int argc = (*env)->GetArrayLength(env, args);
  JSValue *argv = NULL;
  jlong *argPtrs = NULL;

  if (argc > 0) {
    argv = malloc(sizeof(JSValue) * argc);
    argPtrs = (*env)->GetLongArrayElements(env, args, NULL);
    for (int i = 0; i < argc; i++) {
      JSValue *arg = (JSValue *)argPtrs[i];
      argv[i] = *arg;
    }
  }

  JSValue result = JS_Call(ctx, *func, this_val, argc, argv);

  if (argPtrs) {
    (*env)->ReleaseLongArrayElements(env, args, argPtrs, JNI_ABORT);
  }
  if (argv) {
    free(argv);
  }

  if (JS_IsException(result)) {
    JSValue exception_val = JS_GetException(ctx);
    const char *str_res = JS_ToCString(ctx, exception_val);
    JS_FreeValue(ctx, exception_val);

    jclass excCls = (*env)->FindClass(env, "com/quickjs/QuickJSException");
    (*env)->ThrowNew(env, excCls, str_res);

    JS_FreeCString(ctx, str_res);
    return 0;
  }

  return boxJSValue(result);
}

JNIEXPORT jlong JNICALL Java_com_quickjs_JSContext_parseJSONInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jstring json) {
  JSContext *ctx = (JSContext *)contextPtr;
  if (!ctx)
    return 0;

  const char *c_json = GetStringUTFChars(env, json);
  if (!c_json)
    return 0;

  JSValue val = JS_ParseJSON(ctx, c_json, strlen(c_json), "<input>");

  ReleaseStringUTFChars(env, json, c_json);

  if (JS_IsException(val)) {
    JSValue exception_val = JS_GetException(ctx);
    const char *str_res = JS_ToCString(ctx, exception_val);
    JS_FreeValue(ctx, exception_val);
    JS_FreeValue(ctx, val); // Free the exception value returned by ParseJSON
                            // (it is JS_EXCEPTION)

    jclass excCls = (*env)->FindClass(env, "com/quickjs/QuickJSException");
    (*env)->ThrowNew(env, excCls, str_res);

    JS_FreeCString(ctx, str_res);
    return 0;
  }

  return boxJSValue(val);
}

JNIEXPORT jstring JNICALL Java_com_quickjs_JSValue_toJSONInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jlong valPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *v = (JSValue *)valPtr;

  JSValue jsonStrVal = JS_JSONStringify(ctx, *v, JS_UNDEFINED, JS_UNDEFINED);

  if (JS_IsException(jsonStrVal)) {
    JSValue exception_val = JS_GetException(ctx);
    const char *str_res = JS_ToCString(ctx, exception_val);
    JS_FreeValue(ctx, exception_val);

    jclass excCls = (*env)->FindClass(env, "com/quickjs/QuickJSException");
    (*env)->ThrowNew(env, excCls, str_res);

    JS_FreeCString(ctx, str_res);
    return NULL;
  }

  const char *str = JS_ToCString(ctx, jsonStrVal);
  jstring res = (*env)->NewStringUTF(env, str);

  JS_FreeCString(ctx, str);
  JS_FreeValue(ctx, jsonStrVal);

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
