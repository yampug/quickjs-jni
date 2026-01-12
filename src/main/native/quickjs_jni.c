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

// Cached JNI entries
static jclass g_JSFunctionClass;
static jmethodID g_JSFunction_apply;
static jclass g_JSValueClass;
static jmethodID g_JSValue_ctor;
static jfieldID g_JSValue_ptr;

// Cached Exception Classes
static jclass g_QuickJSExceptionClass;
static jclass g_JSSyntaxErrorClass;
static jclass g_JSReferenceErrorClass;
static jclass g_JSTypeErrorClass;
static jclass g_JSRangeErrorClass;
static jclass g_JSInternalErrorClass;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
  g_vm = vm;
  JNIEnv *env;
  if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
    return JNI_ERR;
  }

  // Cache JSFunction
  jclass localJSFn = (*env)->FindClass(env, "com/quickjs/JSFunction");
  if (!localJSFn)
    goto error;
  g_JSFunctionClass = (*env)->NewGlobalRef(env, localJSFn);
  (*env)->DeleteLocalRef(env, localJSFn);
  if (!g_JSFunctionClass)
    goto error;

  g_JSFunction_apply =
      (*env)->GetMethodID(env, g_JSFunctionClass, "apply",
                          "(Lcom/quickjs/JSContext;Lcom/quickjs/JSValue;[Lcom/"
                          "quickjs/JSValue;)Lcom/quickjs/JSValue;");
  if (!g_JSFunction_apply)
    goto error;

  // Cache JSValue
  jclass localJSVal = (*env)->FindClass(env, "com/quickjs/JSValue");
  if (!localJSVal)
    goto error;
  g_JSValueClass = (*env)->NewGlobalRef(env, localJSVal);
  (*env)->DeleteLocalRef(env, localJSVal);
  if (!g_JSValueClass)
    goto error;

  g_JSValue_ctor = (*env)->GetMethodID(env, g_JSValueClass, "<init>",
                                       "(JLcom/quickjs/JSContext;)V");
  if (!g_JSValue_ctor)
    goto error;

  g_JSValue_ptr = (*env)->GetFieldID(env, g_JSValueClass, "ptr", "J");
  if (!g_JSValue_ptr)
    goto error;

  // Cache Exception Classes
  jclass localEx;

// Helper macro for caching exceptions
#define CACHE_EX(clsName, globalVar)                                           \
  localEx = (*env)->FindClass(env, clsName);                                   \
  if (!localEx)                                                                \
    goto error;                                                                \
  globalVar = (*env)->NewGlobalRef(env, localEx);                              \
  (*env)->DeleteLocalRef(env, localEx);                                        \
  if (!globalVar)                                                              \
    goto error;

  CACHE_EX("com/quickjs/QuickJSException", g_QuickJSExceptionClass);
  CACHE_EX("com/quickjs/JSSyntaxError", g_JSSyntaxErrorClass);
  CACHE_EX("com/quickjs/JSReferenceError", g_JSReferenceErrorClass);
  CACHE_EX("com/quickjs/JSTypeError", g_JSTypeErrorClass);
  CACHE_EX("com/quickjs/JSRangeError", g_JSRangeErrorClass);
  CACHE_EX("com/quickjs/JSInternalError", g_JSInternalErrorClass);

#undef CACHE_EX

  return JNI_VERSION_1_6;

error:
  // Cleanup whatever was allocated
  if (g_JSFunctionClass)
    (*env)->DeleteGlobalRef(env, g_JSFunctionClass);
  if (g_JSValueClass)
    (*env)->DeleteGlobalRef(env, g_JSValueClass);
  if (g_QuickJSExceptionClass)
    (*env)->DeleteGlobalRef(env, g_QuickJSExceptionClass);
  if (g_JSSyntaxErrorClass)
    (*env)->DeleteGlobalRef(env, g_JSSyntaxErrorClass);
  if (g_JSReferenceErrorClass)
    (*env)->DeleteGlobalRef(env, g_JSReferenceErrorClass);
  if (g_JSTypeErrorClass)
    (*env)->DeleteGlobalRef(env, g_JSTypeErrorClass);
  if (g_JSRangeErrorClass)
    (*env)->DeleteGlobalRef(env, g_JSRangeErrorClass);
  if (g_JSInternalErrorClass)
    (*env)->DeleteGlobalRef(env, g_JSInternalErrorClass);

  return JNI_ERR;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved) {
  JNIEnv *env;
  if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
    return;
  }

  if (g_JSFunctionClass)
    (*env)->DeleteGlobalRef(env, g_JSFunctionClass);
  if (g_JSValueClass)
    (*env)->DeleteGlobalRef(env, g_JSValueClass);
  if (g_QuickJSExceptionClass)
    (*env)->DeleteGlobalRef(env, g_QuickJSExceptionClass);
  if (g_JSSyntaxErrorClass)
    (*env)->DeleteGlobalRef(env, g_JSSyntaxErrorClass);
  if (g_JSReferenceErrorClass)
    (*env)->DeleteGlobalRef(env, g_JSReferenceErrorClass);
  if (g_JSTypeErrorClass)
    (*env)->DeleteGlobalRef(env, g_JSTypeErrorClass);
  if (g_JSRangeErrorClass)
    (*env)->DeleteGlobalRef(env, g_JSRangeErrorClass);
  if (g_JSInternalErrorClass)
    (*env)->DeleteGlobalRef(env, g_JSInternalErrorClass);
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
  if (!p)
    return 0;
  *p = v;
  return (jlong)p;
}

// Helper macros for validaty checks
#define CHECK_PTR(ptr, ret)                                                    \
  if (!ptr)                                                                    \
    return ret;

#define CHECK_CONTEXT(ctx) CHECK_PTR(ctx, 0)
#define CHECK_RUNTIME(rt) CHECK_PTR(rt, 0)

static void throw_java_exception(JNIEnv *env, JSContext *ctx,
                                 JSValue exception_val) {
  // 1. Get exception message
  const char *msg = JS_ToCString(ctx, exception_val);

  // 2. Detect Exception Class
  jclass excCls = g_QuickJSExceptionClass;
  JSValue nameVal = JS_GetPropertyStr(ctx, exception_val, "name");
  if (!JS_IsUndefined(nameVal) && !JS_IsNull(nameVal)) {
    const char *name = JS_ToCString(ctx, nameVal);
    if (name) {
      if (strcmp(name, "SyntaxError") == 0)
        excCls = g_JSSyntaxErrorClass;
      else if (strcmp(name, "ReferenceError") == 0)
        excCls = g_JSReferenceErrorClass;
      else if (strcmp(name, "TypeError") == 0)
        excCls = g_JSTypeErrorClass;
      else if (strcmp(name, "RangeError") == 0)
        excCls = g_JSRangeErrorClass;
      else if (strcmp(name, "InternalError") == 0)
        excCls = g_JSInternalErrorClass;
      JS_FreeCString(ctx, name);
    }
  }
  JS_FreeValue(ctx, nameVal);

  // 3. Get Stack
  const char *stack = NULL;
  JSValue stackVal = JS_GetPropertyStr(ctx, exception_val, "stack");
  if (!JS_IsUndefined(stackVal)) {
    stack = JS_ToCString(ctx, stackVal);
  }
  JS_FreeValue(ctx, stackVal);

  // 4. Construct Full Message
  char *full_msg = NULL;
  if (msg && stack && strlen(stack) > 0) {
    size_t len = strlen(msg) + strlen(stack) + 2;
    full_msg = malloc(len);
    if (full_msg)
      snprintf(full_msg, len, "%s\n%s", msg, stack);
  }
  const char *final_msg = full_msg ? full_msg : (msg ? msg : "Unknown Error");

  // 5. Throw Java Exception
  if (excCls) {
    (*env)->ThrowNew(env, excCls, final_msg);
  } else {
    (*env)->FatalError(env, "QuickJS JNI: Exception class corrupted");
  }

  // Clean up
  if (msg)
    JS_FreeCString(ctx, msg);
  if (stack)
    JS_FreeCString(ctx, stack);
  if (full_msg)
    free(full_msg);
}

static void check_throw_exception(JNIEnv *env, JSContext *ctx, JSValue val) {
  if (JS_IsException(val)) {
    JSValue exception_val = JS_GetException(ctx);
    throw_java_exception(env, ctx, exception_val);
    JS_FreeValue(ctx, exception_val);
  }
}

typedef struct {
  JSRuntime *rt;
  // Use a simple int flag. 0 = no interrupt, 1 = interrupt.
  volatile int interrupted;
  jobject moduleLoader;
} NativeRuntimeData;

static int js_interrupt_handler(JSRuntime *rt, void *opaque) {
  NativeRuntimeData *data = (NativeRuntimeData *)opaque;
  return data->interrupted;
}

JNIEXPORT jlong JNICALL
Java_com_quickjs_QuickJS_createNativeRuntime(JNIEnv *env, jclass clazz) {
  JSRuntime *rt = JS_NewRuntime();
  if (!rt)
    return 0;

  NativeRuntimeData *data = malloc(sizeof(NativeRuntimeData));
  if (!data) {
    JS_FreeRuntime(rt);
    return 0;
  }
  memset(data, 0, sizeof(NativeRuntimeData));
  data->rt = rt;
  data->interrupted = 0;
  data->moduleLoader = NULL;

  JS_SetRuntimeOpaque(rt, data);
  JS_SetInterruptHandler(rt, js_interrupt_handler, data);

  if (js_java_proxy_class_id == 0) {
    JS_NewClassID(rt, &js_java_proxy_class_id);
  }
  JS_NewClass(rt, js_java_proxy_class_id, &js_java_proxy_class);

  return (jlong)rt;
}

JNIEXPORT void JNICALL Java_com_quickjs_JSRuntime_setMemoryLimitInternal(
    JNIEnv *env, jobject thiz, jlong runtimePtr, jlong limit) {
  JSRuntime *rt = (JSRuntime *)runtimePtr;
  if (rt) {
    JS_SetMemoryLimit(rt, (size_t)limit);
  }
}

JNIEXPORT void JNICALL Java_com_quickjs_JSRuntime_setMaxStackSizeInternal(
    JNIEnv *env, jobject thiz, jlong runtimePtr, jlong size) {
  JSRuntime *rt = (JSRuntime *)runtimePtr;
  if (rt) {
    JS_SetMaxStackSize(rt, (size_t)size);
  }
}

JNIEXPORT void JNICALL Java_com_quickjs_JSRuntime_setInterruptInternal(
    JNIEnv *env, jobject thiz, jlong runtimePtr) {
  JSRuntime *rt = (JSRuntime *)runtimePtr;
  if (rt) {
    NativeRuntimeData *data = (NativeRuntimeData *)JS_GetRuntimeOpaque(rt);
    if (data) {
      data->interrupted = 1;
    }
  }
}

JNIEXPORT void JNICALL Java_com_quickjs_JSRuntime_clearInterruptInternal(
    JNIEnv *env, jobject thiz, jlong runtimePtr) {
  JSRuntime *rt = (JSRuntime *)runtimePtr;
  if (rt) {
    NativeRuntimeData *data = (NativeRuntimeData *)JS_GetRuntimeOpaque(rt);
    if (data) {
      data->interrupted = 0;
    }
  }
}

JSModuleDef *js_java_module_loader(JSContext *ctx, const char *module_name,
                                   void *opaque) {
  NativeRuntimeData *data = (NativeRuntimeData *)opaque;
  if (!data || !data->moduleLoader)
    return NULL;

  jobject loader = data->moduleLoader;
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
    jthrowable ex = (*env)->ExceptionOccurred(env);
    (*env)->ExceptionClear(env);

    jclass exCls = (*env)->GetObjectClass(env, ex);
    jmethodID toString =
        (*env)->GetMethodID(env, exCls, "toString", "()Ljava/lang/String;");
    jstring jMsg = (jstring)(*env)->CallObjectMethod(env, ex, toString);
    const char *cMsg = GetStringUTFChars(env, jMsg);

    JS_ThrowTypeError(ctx, "Java Module Loader failed: %s", cMsg);

    ReleaseStringUTFChars(env, jMsg, cMsg);
    (*env)->DeleteLocalRef(env, jMsg);
    (*env)->DeleteLocalRef(env, exCls);
    (*env)->DeleteLocalRef(env, ex);
    return NULL;
  }

  if (!jContent) {
    return NULL; // Module not found
  }

  const char *content = GetStringUTFChars(env, jContent);
  /* JS_Eval copies the input so we can release the string immediately after. */

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

  NativeRuntimeData *data = (NativeRuntimeData *)JS_GetRuntimeOpaque(rt);
  if (!data)
    return;

  // Manage previous loader
  if (data->moduleLoader) {
    (*env)->DeleteGlobalRef(env, data->moduleLoader);
    data->moduleLoader = NULL;
  }

  if (loader) {
    data->moduleLoader = (*env)->NewGlobalRef(env, loader);
  }

  // We pass 'data' as opaque to the loader func, which extracts moduleLoader
  JS_SetModuleLoaderFunc(rt, NULL, js_java_module_loader, data);
}

JNIEXPORT jlong JNICALL Java_com_quickjs_JSContext_evalInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jstring script,
    jstring fileName, jint type) {

  JSContext *ctx = (JSContext *)contextPtr;
  CHECK_CONTEXT(ctx);

  const char *c_script = GetStringUTFChars(env, script);
  CHECK_PTR(c_script, 0);

  const char *c_filename = GetStringUTFChars(env, fileName);

  JSValue val = JS_Eval(ctx, c_script, strlen(c_script), c_filename, type);

  ReleaseStringUTFChars(env, script, c_script);
  ReleaseStringUTFChars(env, fileName, c_filename);

  check_throw_exception(env, ctx, val);
  if (JS_IsException(val)) {
    return 0;
  }

  return boxJSValue(val);
}

// Update freeNativeRuntime to release loader
JNIEXPORT void JNICALL Java_com_quickjs_JSRuntime_freeNativeRuntime(
    JNIEnv *env, jobject thiz, jlong runtimePtr) {
  JSRuntime *rt = (JSRuntime *)runtimePtr;
  if (rt) {
    NativeRuntimeData *data = (NativeRuntimeData *)JS_GetRuntimeOpaque(rt);
    if (data) {
      if (data->moduleLoader) {
        (*env)->DeleteGlobalRef(env, data->moduleLoader);
      }
      free(data);
    }
    JS_FreeRuntime(rt);
  }
}

// ... Rest of file methods needing restoration ...

JNIEXPORT jlong JNICALL Java_com_quickjs_JSRuntime_createNativeContext(
    JNIEnv *env, jobject thiz, jlong runtimePtr, jboolean withStdLib) {
  JSRuntime *rt = (JSRuntime *)runtimePtr;
  if (!rt)
    return 0;

  JSContext *ctx;
  if (withStdLib) {
    ctx = JS_NewContext(rt);
  } else {
    ctx = JS_NewContextRaw(rt);
    if (ctx) {
      JS_AddIntrinsicBaseObjects(ctx);
      JS_AddIntrinsicEval(ctx);
    }
  }
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
    throw_java_exception(env, ctx, exception_val);
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
    throw_java_exception(env, ctx, exception_val);
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

  check_throw_exception(env, ctx, result);
  if (JS_IsException(result)) {
    return 0;
  }

  return boxJSValue(result);
}

JNIEXPORT jlong JNICALL Java_com_quickjs_JSContext_parseJSONInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jstring json) {
  JSContext *ctx = (JSContext *)contextPtr;
  CHECK_CONTEXT(ctx);

  const char *c_json = GetStringUTFChars(env, json);
  CHECK_PTR(c_json, 0);

  JSValue val = JS_ParseJSON(ctx, c_json, strlen(c_json), "<input>");

  ReleaseStringUTFChars(env, json, c_json);

  check_throw_exception(env, ctx, val);
  if (JS_IsException(val)) {
    return 0;
  }

  return boxJSValue(val);
}

JNIEXPORT jstring JNICALL Java_com_quickjs_JSValue_toJSONInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jlong valPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *v = (JSValue *)valPtr;

  JSValue jsonStrVal = JS_JSONStringify(ctx, *v, JS_UNDEFINED, JS_UNDEFINED);

  check_throw_exception(env, ctx, jsonStrVal);
  if (JS_IsException(jsonStrVal)) {
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

  // Use cached classes and methods
  jlong thisPtr = boxJSValue(JS_DupValue(ctx, this_val));
  if (thisPtr == 0)
    return JS_ThrowInternalError(ctx, "Native Error: OOM in boxJSValue");

  jobject jThis = (*env)->NewObject(env, g_JSValueClass, g_JSValue_ctor,
                                    thisPtr, javaContext);
  if (!jThis) {
    free((void *)thisPtr);
    return JS_ThrowInternalError(ctx,
                                 "JNI Error: Failed to create 'this' JSValue");
  }

  jobjectArray jArgs = (*env)->NewObjectArray(env, argc, g_JSValueClass, NULL);
  if (!jArgs) {
    (*env)->DeleteLocalRef(env, jThis);
    return JS_ThrowInternalError(ctx,
                                 "JNI Error: Failed to create argument array");
  }

  for (int i = 0; i < argc; i++) {
    jlong argPtr = boxJSValue(JS_DupValue(ctx, argv[i]));
    if (argPtr == 0) {
      (*env)->DeleteLocalRef(env, jThis);
      (*env)->DeleteLocalRef(env, jArgs);
      return JS_ThrowInternalError(ctx, "Native Error: OOM in argument boxing");
    }
    jobject jArg = (*env)->NewObject(env, g_JSValueClass, g_JSValue_ctor,
                                     argPtr, javaContext);
    if (!jArg) {
      free((void *)argPtr);
      (*env)->DeleteLocalRef(env, jThis);
      (*env)->DeleteLocalRef(env, jArgs);
      return JS_ThrowInternalError(
          ctx, "JNI Error: Failed to create argument JSValue");
    }
    (*env)->SetObjectArrayElement(env, jArgs, i, jArg);
    (*env)->DeleteLocalRef(env, jArg);
  }

  jobject jResult = (*env)->CallObjectMethod(
      env, javaCallback, g_JSFunction_apply, javaContext, jThis, jArgs);

  (*env)->DeleteLocalRef(env, javaContext);
  (*env)->DeleteLocalRef(env, jThis);
  (*env)->DeleteLocalRef(env, jArgs);

  if ((*env)->ExceptionCheck(env)) {
    jthrowable ex = (*env)->ExceptionOccurred(env);
    (*env)->ExceptionClear(env);

    jclass exCls = (*env)->GetObjectClass(env, ex);
    jstring msg = NULL;

    jmethodID toString =
        (*env)->GetMethodID(env, exCls, "toString", "()Ljava/lang/String;");
    if (toString) {
      msg = (jstring)(*env)->CallObjectMethod(env, ex, toString);
    }

    // Fallback if toString fails
    const char *c_msg = "Unknown Java Exception";
    if (msg) {
      const char *temp_msg = GetStringUTFChars(env, msg);
      if (temp_msg)
        c_msg = temp_msg;
    }

    JSValue err = JS_NewError(ctx);
    JS_DefinePropertyValueStr(ctx, err, "message", JS_NewString(ctx, c_msg),
                              JS_PROP_C_W_E);

    if (msg) {
      const char *temp_msg =
          GetStringUTFChars(env, msg); // Re-get to release properly
      if (temp_msg)
        ReleaseStringUTFChars(env, msg, temp_msg);
      (*env)->DeleteLocalRef(env, msg);
    }
    (*env)->DeleteLocalRef(env, ex);
    (*env)->DeleteLocalRef(env, exCls);

    return JS_Throw(ctx, err);
  }

  if (jResult == NULL) {
    return JS_UNDEFINED;
  }

  jlong resPtr = (*env)->GetLongField(env, jResult, g_JSValue_ptr);

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
  CHECK_CONTEXT(ctx);
  return boxJSValue(JS_NewObject(ctx));
}

JNIEXPORT jlong JNICALL Java_com_quickjs_JSContext_createNullInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  CHECK_CONTEXT(ctx);
  return boxJSValue(JS_NULL);
}

JNIEXPORT jlong JNICALL Java_com_quickjs_JSContext_createUndefinedInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  CHECK_CONTEXT(ctx);
  return boxJSValue(JS_UNDEFINED);
}

JNIEXPORT jlong JNICALL Java_com_quickjs_JSContext_createBooleanInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jboolean v) {
  JSContext *ctx = (JSContext *)contextPtr;
  CHECK_CONTEXT(ctx);
  return boxJSValue(JS_NewBool(ctx, v));
}

JNIEXPORT jlong JNICALL Java_com_quickjs_JSContext_createDoubleInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jdouble v) {
  JSContext *ctx = (JSContext *)contextPtr;
  CHECK_CONTEXT(ctx);
  return boxJSValue(JS_NewFloat64(ctx, v));
}

// Type Checkers
JNIEXPORT jboolean JNICALL Java_com_quickjs_JSValue_isStringInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jlong valPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *v = (JSValue *)valPtr;
  return JS_IsString(*v);
}

JNIEXPORT jboolean JNICALL Java_com_quickjs_JSValue_isNumberInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jlong valPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *v = (JSValue *)valPtr;
  return JS_IsNumber(*v);
}

JNIEXPORT jboolean JNICALL Java_com_quickjs_JSValue_isIntegerInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jlong valPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *v = (JSValue *)valPtr;
  // JS_IsInteger does not exist in public API, check tag
  int tag = JS_VALUE_GET_NORM_TAG(*v);
  return tag == JS_TAG_INT;
}

JNIEXPORT jboolean JNICALL Java_com_quickjs_JSValue_isBooleanInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jlong valPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *v = (JSValue *)valPtr;
  return JS_IsBool(*v);
}

JNIEXPORT jboolean JNICALL Java_com_quickjs_JSValue_isArrayInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jlong valPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *v = (JSValue *)valPtr;
  return JS_IsArray(*v);
}

JNIEXPORT jboolean JNICALL Java_com_quickjs_JSValue_isObjectInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jlong valPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *v = (JSValue *)valPtr;
  return JS_IsObject(*v);
}

JNIEXPORT jboolean JNICALL Java_com_quickjs_JSValue_isFunctionInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jlong valPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *v = (JSValue *)valPtr;
  return JS_IsFunction(ctx, *v);
}

JNIEXPORT jboolean JNICALL Java_com_quickjs_JSValue_isErrorInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jlong valPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *v = (JSValue *)valPtr;
  return JS_IsError(*v);
}

JNIEXPORT jboolean JNICALL Java_com_quickjs_JSValue_isNullInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jlong valPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *v = (JSValue *)valPtr;
  return JS_IsNull(*v);
}

JNIEXPORT jboolean JNICALL Java_com_quickjs_JSValue_isUndefinedInternal(
    JNIEnv *env, jobject thiz, jlong contextPtr, jlong valPtr) {
  JSContext *ctx = (JSContext *)contextPtr;
  JSValue *v = (JSValue *)valPtr;
  return JS_IsUndefined(*v);
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
    throw_java_exception(env, ctx, ex);
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
