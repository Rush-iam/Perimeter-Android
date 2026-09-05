#include <jni.h>
#include <string>
#include <version.h>

// Include engine headers
#include <SystemUtil.h>

extern "C" JNIEXPORT jstring JNICALL
Java_com_queststoredb_perimeter_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {

    // Call an engine function to verify linking
    bool isGo = applicationIsGo();

    std::string hello = "Hello from C++ (Perimeter v";
    hello += VERSION;
    hello += isGo ? ", App is Go)" : ", App not Go)";

    return env->NewStringUTF(hello.c_str());
}

