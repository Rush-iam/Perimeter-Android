package com.queststoredb.perimeter

import android.util.Log

/** Queries the Vulkan loader/physical device rather than Android's feature metadata. */
internal object AndroidVulkanCapabilities {
    private const val LOG_TAG = "PerimeterVulkan"
    private const val PROBE_LIBRARY = "perimeter_vulkan_probe"

    // VK_MAKE_API_VERSION(0, 1, 1, 0) and VK_MAKE_API_VERSION(0, 1, 3, 0).
    private const val VULKAN_1_1_VERSION = 0x00401000
    private const val VULKAN_1_3_VERSION = 0x00403000

    private val nativeLibraryLoaded = try {
        System.loadLibrary(PROBE_LIBRARY)
        true
    } catch (error: UnsatisfiedLinkError) {
        Log.w(LOG_TAG, "Unable to load Vulkan capability probe", error)
        false
    }

    fun supportsVulkan11(): Boolean = supportedApiVersion() >= VULKAN_1_1_VERSION

    fun supportsVulkan13(): Boolean = supportedApiVersion() >= VULKAN_1_3_VERSION

    private fun supportedApiVersion(): Int =
        if (nativeLibraryLoaded) {
            runCatching { nativeSupportedApiVersion() }.getOrElse {
                Log.w(LOG_TAG, "Vulkan capability probe failed", it)
                0
            }
        } else {
            0
        }

    private external fun nativeSupportedApiVersion(): Int
}
