plugins {
    alias(libs.plugins.android.application)
}

android {
    namespace = "com.queststoredb.perimeter.xrsmoke"
    compileSdk = 37
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "com.queststoredb.perimeter.xrsmoke"
        minSdk = 29
        targetSdk = 37
        versionCode = 1
        versionName = "1.0"
        ndk { abiFilters.add("arm64-v8a") }
    }

    buildFeatures { prefab = true }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}

dependencies {
    implementation(libs.openxr.loader.android)
}
