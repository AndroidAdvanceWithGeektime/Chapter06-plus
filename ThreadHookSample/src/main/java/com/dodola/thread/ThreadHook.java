package com.dodola.thread;

import java.util.ArrayList;
import java.util.ListIterator;

public final class ThreadHook {
    static {
        System.loadLibrary("threadhook");
    }

    private static boolean sHasHook = false;
    private static boolean sHookFailed = false;


    public static String getStack() {
        return stackTraceToString(new Throwable().getStackTrace());
    }

    private static String stackTraceToString(final StackTraceElement[] arr) {
        if (arr == null) {
            return "";
        }

        StringBuffer sb = new StringBuffer();

        for (StackTraceElement stackTraceElement : arr) {
            String className = stackTraceElement.getClassName();
            // remove unused stacks
            if (className.contains("java.lang.Thread")) {
                continue;
            }

            sb.append(stackTraceElement).append('\n');
        }
        return sb.toString();
    }

    public static void enableThreadHook() {
        if (sHasHook) {
            return;
        }
        sHasHook = true;
        enableThreadHookNative();

    }




    private static native void enableThreadHookNative();


}