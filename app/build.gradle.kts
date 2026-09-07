plugins {
    alias(libs.plugins.android.application)
}
android {
    namespace = "com.queststoredb.perimeter"
    compileSdk {
        version = release(37)
    }

    defaultConfig {
        applicationId = "com.queststoredb.perimeter"
        minSdk = 29
        targetSdk = 37
        versionCode = 1
        versionName = "1.0"
        ndk {
            abiFilters.add("arm64-v8a")
        }
        @Suppress("UnstableApiUsage")
        externalNativeBuild {
            cmake {
                arguments += "-DANDROID_STL=c++_shared"
                providers.gradleProperty("androidDxvk").orNull?.takeIf { it.isNotBlank() }?.let {
                    arguments += "-DPERIMETER_ANDROID_DXVK=$it"
                }
                for ((property, variable) in mapOf(
                    "dxvkPython" to "ANDROID_DXVK_PYTHON",
                    "dxvkMeson" to "ANDROID_DXVK_MESON",
                    "dxvkGlslang" to "ANDROID_DXVK_GLSLANG",
                    "dxvkSource" to "FETCHCONTENT_SOURCE_DIR_ANDROID_DXVK_SOURCE",
                    "dxvkSdlSource" to "FETCHCONTENT_SOURCE_DIR_SDL2",
                    "dxvkSdlNetSource" to "FETCHCONTENT_SOURCE_DIR_SDL2_NET",
                    "dxvkSdlMixerSource" to "FETCHCONTENT_SOURCE_DIR_SDL2_MIXER",
                    "dxvkSdlImageSource" to "FETCHCONTENT_SOURCE_DIR_SDL2_IMAGE",
                    "ffmpegSource" to "FETCHCONTENT_SOURCE_DIR_FFMPEG_PREBUILT",
                    "simpleiniSource" to "FETCHCONTENT_SOURCE_DIR_SIMPLEINI",
                    "peventsSource" to "FETCHCONTENT_SOURCE_DIR_PEVENTS",
                    "gameMathSource" to "FETCHCONTENT_SOURCE_DIR_GAMEMATH",
                    "imguiSource" to "FETCHCONTENT_SOURCE_DIR_IMGUI",
                    "sokolSource" to "FETCHCONTENT_SOURCE_DIR_SOKOL"
                )) {
                    providers.gradleProperty(property).orNull?.takeIf { it.isNotBlank() }?.let {
                        arguments += "-D$variable=${rootProject.file(it).invariantSeparatorsPath}"
                    }
                }
            }
        }
    }

    buildTypes {
        release {
            optimization {
                enable = false
            }
        }
    }
    flavorDimensions += "renderer"
    productFlavors {
        create("sokolDxvk1") {
            dimension = "renderer"
            buildConfigField("String", "DXVK_VERSION", "\"1\"")
            externalNativeBuild {
                cmake {
                    arguments += "-DPERIMETER_ANDROID_DXVK=ON"
                    arguments += "-DPERIMETER_ANDROID_DXVK_VERSION=1"
                }
            }
        }
        create("sokolDxvk2") {
            dimension = "renderer"
            buildConfigField("String", "DXVK_VERSION", "\"2\"")
            externalNativeBuild {
                cmake {
                    arguments += "-DPERIMETER_ANDROID_DXVK=ON"
                    arguments += "-DPERIMETER_ANDROID_DXVK_VERSION=2"
                }
            }
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    buildFeatures {
        buildConfig = true
        viewBinding = true
    }
}

dependencies {
    implementation(libs.androidx.appcompat)
    implementation(libs.androidx.constraintlayout)
    implementation(libs.androidx.core.ktx)
    implementation(libs.material)
}
