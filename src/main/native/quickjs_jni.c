#include "quickjs.h"
#include <jni.h>
#include <stdlib.h>
#include <string.h>

static JavaVM *g_vm;
static JSClassID js_java_proxy_class_id;

static void js_java_proxy_finalizer(JSRuntime *rt, JSValue val) {
  jobject javaObj = (jobject)JS_GetOpaque(val, js_java_proxy_class_id);
  if (javaObj) {
    JNIEnv *env;
    if ((*g_vm)->GetEnv(g_vm, (void **)&env, JNI_VERSION_1_6) == JNI_OK) {
      (*env)->DeleteGlobalRef(env, javaObj);
    }
  }
}

static JSClassDef js_java_proxy_class = {
    "JavaProxy",
    .finalizer = js_java_proxy_finalizer,
};

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
  g_vm = vm;
  return JNI_VERSION_1_6;
}

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

static jlong boxJSValue(JSValue v) {
  JSValue *p = malloc(sizeof(JSValue));
  *p = v;
  return (jlong)p;
}

JNIEXPORT jlong JNICALL
Java_com_quickjs_QuickJS_createNativeRuntime(JNIEnv *env, jclass clazz) {
  JSRuntime *rt = JS_NewRuntime();
  if (js_java_proxy_class_id == 0) {
    JS_NewClassID(rt, &js_java_proxy_class_id);
  }
  JS_NewClass(rt, js_java_proxy_class_id, &js_java_proxy_class);
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

JNIEXPORT void JNICALL Java_com_quickjs_JSContext_registerJavaContext(
    JNIEnv *env, jobject thiz, jlong contextPtr, jobject javaContext) {
  JSContext *ctx = (JSContext *)contextPtr;
  if (!ctx)
    return;
  jweak globalCtx = (*env)->NewWeakGlobalRef(env, javaContext);
  JS_SetContextOpaque(ctx, globalCtx);
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
    JS_FreeValue(ctx, JS_GetException(ctx));
    return 0.0;
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
    JS_FreeValue(ctx, val);

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

static JSValue callback_trampoline(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv, int magic,
                                   JSValue *func_data) {
  JSValue proxy = func_data[0];
  jobject javaCallback = (jobject)JS_GetOpaque(proxy, js_java_proxy_class_id);
  if (!javaCallback)
    return JS_UNDEFINED;

  jweak javaContextWeak = (jweak)JS_GetContextOpaque(ctx);
  if (!javaContextWeak)
    return JS_UNDEFINED;

  JNIEnv *env;
  if ((*g_vm)->GetEnv(g_vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
    return JS_ThrowInternalError(ctx, "JNI Env unavailable");
  }

  jobject javaContext = (*env)->NewLocalRef(env, javaContextWeak);
  if (!javaContext) {
    return JS_ThrowInternalError(ctx, "Java JSContext is dead");
  }

  jclass jsFunctionCls = (*env)->FindClass(env, "com/quickjs/JSFunction");
  jmethodID applyMethod =
      (*env)->GetMethodID(env, jsFunctionCls, "apply",
                          "(Lcom/quickjs/JSContext;Lcom/quickjs/JSValue;[Lcom/"
                          "quickjs/JSValue;)Lcom/quickjs/JSValue;");

  jclass jsValueCls = (*env)->FindClass(env, "com/quickjs/JSValue");
  jmethodID jsValueCtor = (*env)->GetMethodID(env, jsValueCls, "<init>",
                                              "(JLcom/quickjs/JSContext;)V");

  jlong thisPtr = boxJSValue(JS_DupValue(ctx, this_val));
  jobject jThis =
      (*env)->NewObject(env, jsValueCls, jsValueCtor, thisPtr, javaContext);

  jobjectArray jArgs = (*env)->NewObjectArray(env, argc, jsValueCls, NULL);
  for (int i = 0; i < argc; i++) {
    jlong argPtr = boxJSValue(JS_DupValue(ctx, argv[i]));
    jobject jArg =
        (*env)->NewObject(env, jsValueCls, jsValueCtor, argPtr, javaContext);
    (*env)->SetObjectArrayElement(env, jArgs, i, jArg);
    (*env)->DeleteLocalRef(env, jArg);
  }

  jobject jResult = (*env)->CallObjectMethod(env, javaCallback, applyMethod,
                                             javaContext, jThis, jArgs);

  (*env)->DeleteLocalRef(env, javaContext);
  (*env)->DeleteLocalRef(env, jThis);
  (*env)->DeleteLocalRef(env, jArgs);

  if ((*env)->ExceptionCheck(env)) {
    jthrowable ex = (*env)->ExceptionOccurred(env);
    (*env)->ExceptionClear(env);

    jclass exCls = (*env)->GetObjectClass(env, ex);
    jmethodID toString =
        (*env)->GetMethodID(env, exCls, "toString", "()Ljava/lang/String;");
    jstring msg = (jstring)(*env)->CallObjectMethod(env, ex, toString);
    const char *c_msg = GetStringUTFChars(env, msg);

    JSValue err = JS_NewError(ctx);
    JS_DefinePropertyValueStr(ctx, err, "message", JS_NewString(ctx, c_msg),
                              JS_PROP_C_W_E);

    ReleaseStringUTFChars(env, msg, c_msg);
    (*env)->DeleteLocalRef(env, ex);
    (*env)->DeleteLocalRef(env, msg);
    (*env)->DeleteLocalRef(env, exCls);

    return JS_Throw(ctx, err);
  }

  if (jResult == NULL) {
    return JS_UNDEFINED;
  }

  jfieldID ptrField = (*env)->GetFieldID(env, jsValueCls, "ptr", "J");
  jlong resPtr = (*env)->GetLongField(env, jResult, ptrField);

  JSValue *resValPtr = (JSValue *)resPtr;
  JSValue resVal = JS_DupValue(ctx, *resValPtr);

  (*env)->DeleteLocalRef(env, jResult);

  return resVal;
}

JNIEXPORT jlong JNICALL Java_com_quickjs_JSContext_createFunctionInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jobject callback, jstring name,
    jint argCount) {
  JSContext *ctx = (JSContext *)contextPtr;
  if (!ctx)
    return 0;

  jobject cbGlobal = (*env)->NewGlobalRef(env, callback);

  JSValue proxy = JS_NewObjectClass(ctx, js_java_proxy_class_id);
  JS_SetOpaque(proxy, cbGlobal);

  const char *c_name = GetStringUTFChars(env, name);

  JSValue func_data[1];
  func_data[0] = proxy;

  JSValue func =
      JS_NewCFunctionData(ctx, callback_trampoline, argCount, 0, 1, func_data);
  JS_DefinePropertyValueStr(ctx, func, "name", JS_NewString(ctx, c_name),
                            JS_PROP_CONFIGURABLE);

  ReleaseStringUTFChars(env, name, c_name);

  JS_FreeValue(ctx, proxy);

  return boxJSValue(func);
}

JNIEXPORT jlong JNICALL Java_com_quickjs_JSContext_createIntegerInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jint value) {
  JSContext *ctx = (JSContext *)contextPtr;
  if (!ctx)
    return 0;
  return boxJSValue(JS_NewInt32(ctx, value));
}

JNIEXPORT jlong JNICALL Java_com_quickjs_JSContext_createStringInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jstring value) {
  JSContext *ctx = (JSContext *)contextPtr;
  if (!ctx || !value)
    return 0;

  const char *c_str = GetStringUTFChars(env, value);
  JSValue val = JS_NewString(ctx, c_str);
  ReleaseStringUTFChars(env, value, c_str);

  return boxJSValue(val);
}

JNIEXPORT jlong JNICALL Java_com_quickjs_JSContext_getGlobalObjectInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  if (!ctx)
    return 0;
  return boxJSValue(JS_GetGlobalObject(ctx));
}

JNIEXPORT jlong JNICALL Java_com_quickjs_JSContext_createArrayInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  if (!ctx)
    return 0;
  return boxJSValue(JS_NewArray(ctx));
}

JNIEXPORT jlong JNICALL Java_com_quickjs_JSContext_createObjectInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  if (!ctx)
    return 0;
  return boxJSValue(JS_NewObject(ctx));
}

JNIEXPORT jobjectArray JNICALL Java_com_quickjs_JSValue_getKeysInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jlong valPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *obj = (JSValue *)valPtr;

  if (!ctx || !obj)
    return NULL;

  JSPropertyEnum *tab;
  uint32_t len;

  if (JS_GetOwnPropertyNames(ctx, &tab, &len, *obj,
                             JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK |
                                 JS_GPN_ENUM_ONLY) == -1) {
    return NULL;
  }

  jclass stringCls = (*env)->FindClass(env, "java/lang/String");
  jobjectArray keys = (*env)->NewObjectArray(env, len, stringCls, NULL);

  for (uint32_t i = 0; i < len; i++) {
    JSValue val = JS_AtomToValue(ctx, tab[i].atom);
    const char *str = JS_ToCString(ctx, val);
    jstring jstr = (*env)->NewStringUTF(env, str);

    (*env)->SetObjectArrayElement(env, keys, i, jstr);

    (*env)->DeleteLocalRef(env, jstr);
    JS_FreeCString(ctx, str);
    JS_FreeValue(ctx, val);
  }

  js_free(ctx, tab);

  return keys;
}
