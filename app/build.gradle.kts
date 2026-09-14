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
                val androidDxvk = providers.gradleProperty("androidDxvk").orNull
                    ?.takeIf { it.isNotBlank() } ?: "ON"
                arguments += "-DPERIMETER_ANDROID_DXVK=$androidDxvk"
                val sharedDependencySourcesEnabled =
                    !providers.gradleProperty("androidSharedDependencySources").orNull
                        .equals("false", ignoreCase = true)
                if (sharedDependencySourcesEnabled) {
                    val sharedDependencySourceDir =
                        providers.gradleProperty("androidDependencySourceDir").orNull
                            ?.takeIf { it.isNotBlank() }
                            ?.let(rootProject::file)
                            ?: gradle.gradleUserHomeDir.resolve("caches/perimeter-android/sources")
                    arguments += "-DPERIMETER_ANDROID_DEPENDENCY_SOURCE_DIR=${sharedDependencySourceDir.invariantSeparatorsPath}"
                } else {
                    arguments += "-DPERIMETER_ANDROID_DEPENDENCY_SOURCE_DIR="
                }
                val compilerCacheMode = providers.gradleProperty("androidCompilerCache")
                    .orNull?.takeIf { it.isNotBlank() } ?: "AUTO"
                arguments += "-DANDROID_COMPILER_CACHE=$compilerCacheMode"
                val compilerCacheDir = providers.gradleProperty("androidCompilerCacheDir").orNull
                    ?.takeIf { it.isNotBlank() }
                    ?.let(rootProject::file)
                    ?: gradle.gradleUserHomeDir.resolve("caches/perimeter-android/compiler-cache")
                arguments += "-DANDROID_COMPILER_CACHE_DIR=${compilerCacheDir.invariantSeparatorsPath}"
                providers.gradleProperty("androidCompilerCacheExecutable").orNull
                    ?.takeIf { it.isNotBlank() }?.let {
                        arguments += "-DANDROID_COMPILER_CACHE_EXECUTABLE=${rootProject.file(it).invariantSeparatorsPath}"
                    }
                for ((property, variable) in mapOf(
                    "dxvkPython" to "ANDROID_DXVK_PYTHON",
                    "dxvkMeson" to "ANDROID_DXVK_MESON",
                    "dxvkGlslang" to "ANDROID_DXVK_GLSLANG",
                    "boostSource" to "FETCHCONTENT_SOURCE_DIR_BOOST_HEADERS",
                    "dxvk1Source" to "FETCHCONTENT_SOURCE_DIR_ANDROID_DXVK_SOURCE_V1",
                    "dxvk2Source" to "FETCHCONTENT_SOURCE_DIR_ANDROID_DXVK_SOURCE_V2",
                    "dxvkSdlSource" to "FETCHCONTENT_SOURCE_DIR_SDL2",
                    "dxvkSdlNetSource" to "FETCHCONTENT_SOURCE_DIR_SDL2_NET",
                    "dxvkSdlMixerSource" to "FETCHCONTENT_SOURCE_DIR_SDL2_MIXER",
                    "dxvkSdlImageSource" to "FETCHCONTENT_SOURCE_DIR_SDL2_IMAGE",
                    "ffmpegSource" to "FETCHCONTENT_SOURCE_DIR_FFMPEG_PREBUILT",
                    "mesonSource" to "FETCHCONTENT_SOURCE_DIR_MESON_SRC",
                    "glslangSource" to "FETCHCONTENT_SOURCE_DIR_GLSLANG_BIN",
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
            signingConfig = signingConfigs.getByName("debug")
            optimization {
                enable = false
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
        prefab = true
        viewBinding = true
    }
}

dependencies {
    implementation(libs.androidx.appcompat)
    implementation(libs.androidx.constraintlayout)
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.games.frame.pacing)
    implementation(libs.material)
}
