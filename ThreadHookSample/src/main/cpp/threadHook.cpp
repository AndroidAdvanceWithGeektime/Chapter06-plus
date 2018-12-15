#include <jni.h>
#include <string>

#include <atomic>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sstream>
#include <android/log.h>
#include <unordered_set>
#include <fcntl.h>
#include <sys/fcntl.h>
#include <stdlib.h>
#include <libgen.h>
#include <syscall.h>
#include "linker.h"
#include "hooks.h"
#include <pthread.h>

#define  LOG_TAG    "HOOOOOOOOK"
#define  ALOG(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

std::atomic<bool> thread_hooked;

static jclass kJavaClass;
static jmethodID kMethodGetStack;
static JavaVM *kJvm;


char *jstringToChars(JNIEnv *env, jstring jstr) {
    if (jstr == nullptr) {
        return nullptr;
    }

    jboolean isCopy = JNI_FALSE;
    const char *str = env->GetStringUTFChars(jstr, &isCopy);
    char *ret = strdup(str);
    env->ReleaseStringUTFChars(jstr, str);
    return ret;
}

void printJavaStack() {
    JNIEnv* jniEnv = NULL;
    // JNIEnv 是绑定线程的，所以这里要重新取
    kJvm->GetEnv((void**)&jniEnv, JNI_VERSION_1_6);
    jstring java_stack = static_cast<jstring>(jniEnv->CallStaticObjectMethod(kJavaClass, kMethodGetStack));
    if (NULL == java_stack) {
        return;
    }
    char* stack = jstringToChars(jniEnv, java_stack);
    ALOG("stack:%s", stack);
    free(stack);

    jniEnv->DeleteLocalRef(java_stack);
}


int pthread_create_hook(pthread_t* thread, const pthread_attr_t* attr,
                            void* (*start_routine) (void *), void* arg) {
    printJavaStack();
    return CALL_PREV(pthread_create_hook, thread, attr, *start_routine, arg);
}


/**
* plt hook libc 的 pthread_create 方法，第一个参数的含义为排除掉 libc.so
*/
void hookLoadedLibs() {
    ALOG("hook_plt_method");
    hook_plt_method("libart.so", "pthread_create", (hook_func) &pthread_create_hook);
}


void enableThreadHook() {
    if (thread_hooked) {
        return;
    }
    ALOG("enableThreadHook");

    thread_hooked = true;
    if (linker_initialize()) {
        throw std::runtime_error("Could not initialize linker library");
    }
    hookLoadedLibs();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_dodola_thread_ThreadHook_enableThreadHookNative(JNIEnv *env, jclass type) {

    enableThreadHook();
}

static bool InitJniEnv(JavaVM *vm) {
    kJvm = vm;
    JNIEnv* env = NULL;
    if (kJvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK){
        ALOG("InitJniEnv GetEnv !JNI_OK");
        return false;
    }
    kJavaClass = reinterpret_cast<jclass>(env->NewGlobalRef(env->FindClass("com/dodola/thread/ThreadHook")));
    if (kJavaClass == NULL)  {
        ALOG("InitJniEnv kJavaClass NULL");
        return false;
    }

    kMethodGetStack = env->GetStaticMethodID(kJavaClass, "getStack", "()Ljava/lang/String;");
    if (kMethodGetStack == NULL) {
        ALOG("InitJniEnv kMethodGetStack NULL");
        return false;
    }
    return true;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved){
    ALOG("JNI_OnLoad");


    if (!InitJniEnv(vm)) {
        return -1;
    }

    return JNI_VERSION_1_6;
}

