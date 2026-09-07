plugins {
    alias(libs.plugins.android.application)
}

android {
    namespace = "com.queststoredb.perimeter.dxvksmoke"
    compileSdk = 37
    ndkVersion = "28.2.13676358"
    defaultConfig {
        applicationId = "com.queststoredb.perimeter.dxvksmoke"
        minSdk = 29
        targetSdk = 37
        versionCode = 1
        versionName = "1.0"
        ndk { abiFilters.add("arm64-v8a") }
        externalNativeBuild {
            cmake {
                arguments += "-DANDROID_STL=c++_shared"
                providers.gradleProperty("dxvkVersion").orNull?.takeIf { it.isNotBlank() }?.let {
                    arguments += "-DPERIMETER_ANDROID_DXVK_VERSION=$it"
                }
                for ((property, variable) in mapOf(
                    "dxvkPython" to "ANDROID_DXVK_PYTHON",
                    "dxvkMeson" to "ANDROID_DXVK_MESON",
                    "dxvkGlslang" to "ANDROID_DXVK_GLSLANG",
                    "dxvkSource" to "FETCHCONTENT_SOURCE_DIR_ANDROID_DXVK_SOURCE",
                    "dxvkSdlSource" to "FETCHCONTENT_SOURCE_DIR_SDL2"
                )) {
                    providers.gradleProperty(property).orNull?.takeIf { it.isNotBlank() }?.let {
                        arguments += "-D$variable=${rootProject.file(it).invariantSeparatorsPath}"
                    }
                }
            }
        }
    }
    sourceSets.getByName("main").java.srcDir("../app/src/main/java/org/libsdl/app")
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}
