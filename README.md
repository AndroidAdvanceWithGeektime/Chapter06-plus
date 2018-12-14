# Chapter06-plus

该项目展示了如何使用 PLTHook 技术来获取线程创建的堆栈

运行环境
=====
AndroidStudio3.2
NDK16~19
支持 `x86` `armeabi-v7a`

说明
====

运行项目后点击`开启 Thread Hook`按钮，然后点击`新建 Thread`按钮。在Logcat 日志中查看到捕获的日志，类似如下：

```
com.dodola.thread.MainActivity$2.onClick(MainActivity.java:33)
    android.view.View.performClick(View.java:5637)
    android.view.View$PerformClick.run(View.java:22429)
    android.os.Handler.handleCallback(Handler.java:751)
    android.os.Handler.dispatchMessage(Handler.java:95)
    android.os.Looper.loop(Looper.java:154)
    android.app.ActivityThread.main(ActivityThread.java:6121)
    java.lang.reflect.Method.invoke(Native Method)
    com.android.internal.os.ZygoteInit$MethodAndArgsCaller.run(ZygoteInit.java:889)
    com.android.internal.os.ZygoteInit.main(ZygoteInit.java:779)
```


实现步骤
====

### 寻找Hook点

我们来先线程的启动流程，可以参考这篇文章[Android线程的创建过程](https://www.jianshu.com/p/a26d11502ec8)

[java_lang_Thread.cc](http://androidxref.com/9.0.0_r3/xref/art/runtime/native/java_lang_Thread.cc#43):Thread_nativeCreate
```
static void Thread_nativeCreate(JNIEnv* env, jclass, jobject java_thread, jlong stack_size, jboolean daemon) {
  Thread::CreateNativeThread(env, java_thread, stack_size, daemon == JNI_TRUE);
}
```

[thread.cc](http://androidxref.com/9.0.0_r3/xref/art/runtime/thread.cc) 中的CreateNativeThread函数

```
void Thread::CreateNativeThread(JNIEnv* env, jobject java_peer, size_t stack_size, bool is_daemon) {
    ...
    pthread_create_result = pthread_create(&new_pthread,
                                             &attr,
                                             Thread::CreateCallback,
                                             child_thread);
    ...
}
```

整个流程就非常简单了，我们可以使用inline hook函数Thread_nativeCreate或者CreateNativeThread。

不过考虑到inline hook的兼容性，我们更希望使用got hook或者plt hook。

pthread_create就是一个非常好的点，我们可以利用它来做文章

### 查找Hook的So
上面Thread_nativeCreate、CreateNativeThread和pthread_create函数分别编译在哪个library中呢？

很简单，我们看看编译脚本[Android.bp](http://androidxref.com/9.0.0_r3/xref/art/runtime/Android.bp)就知道了。

```
art_cc_library {
   name: "libart",
   defaults: ["libart_defaults"],
}

cc_defaults {
   name: "libart_defaults",
   defaults: ["art_defaults"],
   host_supported: true,
   srcs: [
    thread.cc",
   ]
}
```

可以看到是在"libart.so"中，而pthread_create熟悉的人都知道它是在"libc.so"中的。

### 查找Hook函数的符号

C++ 的函数名会 Name Mangling，我们需要看看导出符号。

```
readelf -a libart.so

```

pthread_create函数的确是在libc.so中，而且因为c编译的不需要deMangling

```
001048a0  0007fc16 R_ARM_JUMP_SLOT   00000000   pthread_create@LIBC
```

### 真正实现

剩下的实现就非常简单了，如果你想监控其他so库的pthread_create。

profilo中也有一种做法是把目前已经加载的所有so都统一hook了，考虑到性能问题，我们并没有这么做，而且只hook指定的so.

```
hook_plt_method("libart.so", "pthread_create", (hook_func) &pthread_create_hook);

```

而pthread_create的参数直接查看[pthread.h](http://androidxref.com/9.0.0_r3/xref/bionic/libc/include/pthread.h)就可以了。

```
int pthread_create(pthread_t* __pthread_ptr, pthread_attr_t const* __attr, void* (*__start_routine)(void*), void*);
```

获取堆栈是在native反射Java的方法

```
jstring java_stack = static_cast<jstring>(jniEnv->CallStaticObjectMethod(kJavaClass, kMethodGetStack));
```

可以看到整个流程的确是so easy.