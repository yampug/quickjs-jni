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

static void throwJSException(JNIEnv *env, JSContext *ctx, JSValue ex) {
  // 1. Get exception message
  const char *msg = JS_ToCString(ctx, ex);

  // 2. Detect Exception Class based on 'name' property
  const char *className = "com/quickjs/QuickJSException";
  JSValue nameVal = JS_GetPropertyStr(ctx, ex, "name");
  if (!JS_IsUndefined(nameVal) && !JS_IsNull(nameVal)) {
    const char *name = JS_ToCString(ctx, nameVal);
    if (name) {
      if (strcmp(name, "SyntaxError") == 0)
        className = "com/quickjs/JSSyntaxError";
      else if (strcmp(name, "ReferenceError") == 0)
        className = "com/quickjs/JSReferenceError";
      else if (strcmp(name, "TypeError") == 0)
        className = "com/quickjs/JSTypeError";
      else if (strcmp(name, "RangeError") == 0)
        className = "com/quickjs/JSRangeError";
      else if (strcmp(name, "InternalError") == 0)
        className = "com/quickjs/JSInternalError";
      JS_FreeCString(ctx, name);
    }
  }
  JS_FreeValue(ctx, nameVal);

  // 3. Get Stack Trace
  const char *stack = NULL;
  JSValue stackVal = JS_GetPropertyStr(ctx, ex, "stack");
  if (!JS_IsUndefined(stackVal)) {
    stack = JS_ToCString(ctx, stackVal);
  }
  JS_FreeValue(ctx, stackVal);

  // 4. Construct Full Message
  char *full_msg = NULL;
  if (msg && stack && strlen(stack) > 0) {
    size_t len = strlen(msg) + strlen(stack) + 2;
    full_msg = malloc(len);
    if (full_msg) {
      snprintf(full_msg, len, "%s\n%s", msg, stack);
    }
  }

  const char *final_msg = full_msg ? full_msg : (msg ? msg : "Unknown Error");

  jclass excCls = (*env)->FindClass(env, className);
  if (!excCls) {
    excCls = (*env)->FindClass(env, "com/quickjs/QuickJSException");
  }
  (*env)->ThrowNew(env, excCls, final_msg);

  if (msg)
    JS_FreeCString(ctx, msg);
  if (stack)
    JS_FreeCString(ctx, stack);
  if (full_msg)
    free(full_msg);
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

JSModuleDef *js_java_module_loader(JSContext *ctx, const char *module_name,
                                   void *opaque) {
  jobject loader = (jobject)opaque;
  if (!loader)
    return NULL;

  JNIEnv *env;
  if ((*g_vm)->GetEnv(g_vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
    return NULL;
  }

  // loadModule(String moduleName) -> String
  jclass loaderCls = (*env)->GetObjectClass(env, loader);
  jmethodID loadMethod = (*env)->GetMethodID(
      env, loaderCls, "loadModule", "(Ljava/lang/String;)Ljava/lang/String;");

  jstring jModuleName = (*env)->NewStringUTF(env, module_name);
  jstring jContent =
      (jstring)(*env)->CallObjectMethod(env, loader, loadMethod, jModuleName);

  (*env)->DeleteLocalRef(env, jModuleName);
  (*env)->DeleteLocalRef(env, loaderCls);

  if ((*env)->ExceptionCheck(env)) {
    // Exception in loader -> throw in JS
    // We can't easily propagate the Java exception to the module loader failure
    // failure directly as proper JS error, but returning NULL causes a generic
    // generic load error.
    (*env)->ExceptionClear(env); // Clear it so we don't crash JNI
    return NULL;
  }

  if (!jContent) {
    return NULL; // Module not found
  }

  const char *content = GetStringUTFChars(env, jContent);
  // Takes ownership of buffer? No, JS_Eval creates copy?
  // JS_Eval(ctx, input, input_len, filename, flags)
  // For module loading, we usually return JS_Eval result which is a Module
  // object.

  JSValue val = JS_Eval(ctx, content, strlen(content), module_name,
                        JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);

  ReleaseStringUTFChars(env, jContent, content);
  (*env)->DeleteLocalRef(env, jContent);

  if (JS_IsException(val)) {
    return NULL;
  }

  return (JSModuleDef *)JS_VALUE_GET_PTR(val);
}

JNIEXPORT void JNICALL Java_com_quickjs_JSRuntime_setModuleLoaderInternal(
    JNIEnv *env, jobject thiz, jlong runtimePtr, jobject loader) {
  JSRuntime *rt = (JSRuntime *)runtimePtr;
  if (!rt)
    return;

  // Manage previous loader
  jobject oldLoader = (jobject)JS_GetRuntimeOpaque(rt);
  if (oldLoader) {
    (*env)->DeleteGlobalRef(env, oldLoader);
  }

  jobject newLoader = NULL;
  if (loader) {
    newLoader = (*env)->NewGlobalRef(env, loader);
  }
  JS_SetRuntimeOpaque(rt, newLoader);

  JS_SetModuleLoaderFunc(rt, NULL, js_java_module_loader, newLoader);
}

JNIEXPORT jlong JNICALL Java_com_quickjs_JSContext_evalInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jstring script,
    jstring fileName, jint type) {
  JSContext *ctx = (JSContext *)contextPtr;
  if (!ctx)
    return 0;

  const char *c_script = GetStringUTFChars(env, script);
  if (!c_script)
    return 0;

  const char *c_filename = GetStringUTFChars(env, fileName);

  JSValue val = JS_Eval(ctx, c_script, strlen(c_script), c_filename, type);

  ReleaseStringUTFChars(env, script, c_script);
  ReleaseStringUTFChars(env, fileName, c_filename);

  if (JS_IsException(val)) {
    JSValue exception_val = JS_GetException(ctx);
    throwJSException(env, ctx, exception_val);
    JS_FreeValue(ctx, exception_val);
    JS_FreeValue(ctx, val);
    return 0;
  }

  return boxJSValue(val);
}

// Update freeNativeRuntime to release loader
JNIEXPORT void JNICALL Java_com_quickjs_JSRuntime_freeNativeRuntime(
    JNIEnv *env, jobject thiz, jlong runtimePtr) {
  JSRuntime *rt = (JSRuntime *)runtimePtr;
  if (rt) {
    jobject oldLoader = (jobject)JS_GetRuntimeOpaque(rt);
    if (oldLoader) {
      (*env)->DeleteGlobalRef(env, oldLoader);
    }
    JS_FreeRuntime(rt);
  }
}

// ... Rest of file methods needing restoration ...

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
    throwJSException(env, ctx, exception_val);
    JS_FreeValue(ctx, exception_val);
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
    throwJSException(env, ctx, exception_val);
    JS_FreeValue(ctx, exception_val);
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
    throwJSException(env, ctx, exception_val);
    JS_FreeValue(ctx, exception_val);
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
    throwJSException(env, ctx, exception_val);
    JS_FreeValue(ctx, exception_val);
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
    throwJSException(env, ctx, exception_val);
    JS_FreeValue(ctx, exception_val);
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

JNIEXPORT jboolean JNICALL Java_com_quickjs_JSRuntime_executePendingJobInternal(
    JNIEnv *env, jclass clazz, jlong runtimePtr) {
  JSRuntime *rt = (JSRuntime *)runtimePtr;
  if (!rt)
    return JNI_FALSE;

  JSContext *ctx;
  int err = JS_ExecutePendingJob(rt, &ctx);
  // err: < 0 if exception, 0 if no job, > 0 if job executed

  if (err < 0) {
    // Exception thrown during job execution
    JSValue ex = JS_GetException(ctx);
    // For now, we print it, or we could bubble it up if we had a way.
    // JS_ExecutePendingJob swallows the exception context usually.
    // But we can check it.
    // Ideally we should report this to a "UncaughtException" handler in Java.
    // Printing for now.
    const char *str = JS_ToCString(ctx, ex);
    if (str) {
      fprintf(stderr, "Uncaught exception in Promise: %s\n", str);
      JS_FreeCString(ctx, str);
    }
    JS_FreeValue(ctx, ex);
  }

  return (err > 0) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobjectArray JNICALL
Java_com_quickjs_JSContext_createPromiseCapabilityInternal(JNIEnv *env,
                                                           jobject thiz,
                                                           jlong contextPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  if (!ctx)
    return NULL;

  JSValue resolving_funcs[2];
  JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);

  if (JS_IsException(promise)) {
    return NULL;
  }

  // Return [promise, resolve, reject] (as longs)
  jclass longCls = (*env)->FindClass(env, "java/lang/Long");
  jobjectArray result = (*env)->NewObjectArray(env, 3, longCls, NULL);
  jmethodID longCtor = (*env)->GetMethodID(env, longCls, "<init>", "(J)V");

  jobject jPromise =
      (*env)->NewObject(env, longCls, longCtor, boxJSValue(promise));
  jobject jResolve =
      (*env)->NewObject(env, longCls, longCtor, boxJSValue(resolving_funcs[0]));
  jobject jReject =
      (*env)->NewObject(env, longCls, longCtor, boxJSValue(resolving_funcs[1]));

  (*env)->SetObjectArrayElement(env, result, 0, jPromise);
  (*env)->SetObjectArrayElement(env, result, 1, jResolve);
  (*env)->SetObjectArrayElement(env, result, 2, jReject);

  (*env)->DeleteLocalRef(env, jPromise);
  (*env)->DeleteLocalRef(env, jResolve);
  (*env)->DeleteLocalRef(env, jReject);

  (*env)->DeleteLocalRef(env, jReject);

  return result;
}

JNIEXPORT jlong JNICALL Java_com_quickjs_JSValue_dupInternal(JNIEnv *env,
                                                             jobject thiz,
                                                             jlong contextPtr,
                                                             jlong valPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *v = (JSValue *)valPtr;
  if (!ctx || !v)
    return 0;

  return boxJSValue(JS_DupValue(ctx, *v));
}

JNIEXPORT jboolean JNICALL Java_com_quickjs_JSValue_hasPropertyInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jlong valPtr, jstring key) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *v = (JSValue *)valPtr;
  if (!ctx || !v)
    return JNI_FALSE;

  const char *prop_name = GetStringUTFChars(env, key);
  if (!prop_name)
    return JNI_FALSE;

  JSAtom atom = JS_NewAtom(ctx, prop_name);
  int result = JS_HasProperty(ctx, *v, atom);
  JS_FreeAtom(ctx, atom);

  ReleaseStringUTFChars(env, key, prop_name);
  return result ? JNI_TRUE : JNI_FALSE;
}
