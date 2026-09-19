#include <vulkan/vulkan.h>

#include <dlfcn.h>

#include <algorithm>
#include <mutex>
#include <vector>

#include <android/log.h>
#include <jni.h>

namespace {

constexpr char kLogTag[] = "PerimeterVulkan";

uint32_t querySupportedApiVersion() {
    void* vulkanLibrary = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!vulkanLibrary) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag,
                            "Unable to load libvulkan.so: %s", dlerror());
        return 0;
    }

    const auto getInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        dlsym(vulkanLibrary, "vkGetInstanceProcAddr"));
    if (!getInstanceProcAddr) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag,
                            "libvulkan.so does not export vkGetInstanceProcAddr");
        dlclose(vulkanLibrary);
        return 0;
    }

    auto enumerateInstanceVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
        getInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
    uint32_t instanceApiVersion = VK_API_VERSION_1_0;
    if (enumerateInstanceVersion) {
        const VkResult result = enumerateInstanceVersion(&instanceApiVersion);
        if (result != VK_SUCCESS) {
            __android_log_print(ANDROID_LOG_WARN, kLogTag,
                                "vkEnumerateInstanceVersion failed: %d", result);
            dlclose(vulkanLibrary);
            return 0;
        }
    }

    const auto createInstance = reinterpret_cast<PFN_vkCreateInstance>(
        getInstanceProcAddr(nullptr, "vkCreateInstance"));
    if (!createInstance) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag,
                            "Vulkan loader does not export vkCreateInstance");
        dlclose(vulkanLibrary);
        return 0;
    }

    const VkApplicationInfo applicationInfo{
        VK_STRUCTURE_TYPE_APPLICATION_INFO,
        nullptr,
        "Perimeter Vulkan capability probe",
        VK_MAKE_VERSION(1, 0, 0),
        "Perimeter Android",
        VK_MAKE_VERSION(1, 0, 0),
        VK_API_VERSION_1_0,
    };
    const VkInstanceCreateInfo createInfo{
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        nullptr,
        0,
        &applicationInfo,
        0,
        nullptr,
        0,
        nullptr,
    };

    VkInstance instance = VK_NULL_HANDLE;
    const VkResult createResult = createInstance(&createInfo, nullptr, &instance);
    if (createResult != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag,
                            "vkCreateInstance failed: %d", createResult);
        dlclose(vulkanLibrary);
        return 0;
    }

    const auto enumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        getInstanceProcAddr(instance, "vkEnumeratePhysicalDevices"));
    const auto getPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        getInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties"));
    const auto destroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
        getInstanceProcAddr(instance, "vkDestroyInstance"));

    if (!enumeratePhysicalDevices || !getPhysicalDeviceProperties || !destroyInstance) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag,
                            "Vulkan instance is missing physical-device entry points");
        destroyInstance ? destroyInstance(instance, nullptr) : (void)0;
        dlclose(vulkanLibrary);
        return 0;
    }

    uint32_t deviceCount = 0;
    VkResult result = enumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (result != VK_SUCCESS || deviceCount == 0) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag,
                            "No Vulkan physical devices found: result=%d count=%u",
                            result, deviceCount);
        destroyInstance(instance, nullptr);
        dlclose(vulkanLibrary);
        return 0;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    result = enumeratePhysicalDevices(instance, &deviceCount, devices.data());
    if (result != VK_SUCCESS) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag,
                            "Unable to enumerate Vulkan physical devices: %d", result);
        destroyInstance(instance, nullptr);
        dlclose(vulkanLibrary);
        return 0;
    }

    uint32_t deviceApiVersion = VK_API_VERSION_1_0;
    const char* deviceName = "unknown";
    for (VkPhysicalDevice device : devices) {
        VkPhysicalDeviceProperties properties{};
        getPhysicalDeviceProperties(device, &properties);
        if (properties.apiVersion > deviceApiVersion) {
            deviceApiVersion = properties.apiVersion;
            deviceName = properties.deviceName;
        }
    }

    const uint32_t supportedApiVersion = std::min(instanceApiVersion, deviceApiVersion);
    __android_log_print(
        ANDROID_LOG_INFO, kLogTag,
        "Runtime Vulkan support: instance=%u.%u.%u device=%u.%u.%u (%s) effective=%u.%u.%u",
        VK_VERSION_MAJOR(instanceApiVersion), VK_VERSION_MINOR(instanceApiVersion),
        VK_VERSION_PATCH(instanceApiVersion), VK_VERSION_MAJOR(deviceApiVersion),
        VK_VERSION_MINOR(deviceApiVersion), VK_VERSION_PATCH(deviceApiVersion), deviceName,
        VK_VERSION_MAJOR(supportedApiVersion), VK_VERSION_MINOR(supportedApiVersion),
        VK_VERSION_PATCH(supportedApiVersion));

    destroyInstance(instance, nullptr);
    dlclose(vulkanLibrary);
    return supportedApiVersion;
}

uint32_t cachedSupportedApiVersion() {
    static std::once_flag once;
    static uint32_t version = 0;
    std::call_once(once, [] { version = querySupportedApiVersion(); });
    return version;
}

} // namespace

extern "C" JNIEXPORT jint JNICALL
Java_com_queststoredb_perimeter_AndroidVulkanCapabilities_nativeSupportedApiVersion(
    JNIEnv*, jobject) {
    return static_cast<jint>(cachedSupportedApiVersion());
}
