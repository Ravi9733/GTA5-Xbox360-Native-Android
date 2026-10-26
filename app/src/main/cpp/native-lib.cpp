#include <jni.h>

extern "C" JNIEXPORT jstring JNICALL
Java_com_gta5_nativeandroid_MainActivity_nativeBuildStatus(JNIEnv* env, jobject /* thiz */) {
    constexpr char status[] =
        "ARM64 JNI native library loaded successfully.\n"
        "Game engine integration: NOT IMPLEMENTED.";
    return env->NewStringUTF(status);
}
