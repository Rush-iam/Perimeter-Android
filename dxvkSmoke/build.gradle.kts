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
                providers.gradleProperty("dxvkPython").orNull?.takeIf { it.isNotBlank() }?.let {
                    arguments += "-DANDROID_DXVK_PYTHON=${rootProject.file(it).invariantSeparatorsPath}"
                }
                providers.gradleProperty("dxvkMeson").orNull?.takeIf { it.isNotBlank() }?.let {
                    arguments += "-DANDROID_DXVK_MESON=${rootProject.file(it).invariantSeparatorsPath}"
                }
                providers.gradleProperty("dxvkGlslang").orNull?.takeIf { it.isNotBlank() }?.let {
                    arguments += "-DANDROID_DXVK_GLSLANG=${rootProject.file(it).invariantSeparatorsPath}"
                }
                for ((property, variable) in mapOf(
                    "dxvk1Source" to "FETCHCONTENT_SOURCE_DIR_ANDROID_DXVK_SOURCE_V1",
                    "dxvk2Source" to "FETCHCONTENT_SOURCE_DIR_ANDROID_DXVK_SOURCE_V2",
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
    buildFeatures {
        prefab = true
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}

dependencies {
    implementation(libs.androidx.games.frame.pacing)
}
