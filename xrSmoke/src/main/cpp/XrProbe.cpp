#define XR_USE_PLATFORM_ANDROID
#define XR_USE_GRAPHICS_API_VULKAN
#include <jni.h>
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace {

void version(std::ostringstream& out, XrVersion value) {
    out << XR_VERSION_MAJOR(value) << '.' << XR_VERSION_MINOR(value) << '.' << XR_VERSION_PATCH(value);
}

bool result(std::ostringstream& out, const char* operation, XrResult code) {
    if (XR_SUCCEEDED(code)) return true;
    out << "FAIL " << operation << ": XrResult " << static_cast<int>(code) << '\n';
    return false;
}

void probeGraphics(std::ostringstream& out, XrInstance instance, XrSystemId system) {
    PFN_xrCreateVulkanInstanceKHR createVulkanInstance = nullptr;
    PFN_xrGetVulkanGraphicsDevice2KHR getGraphicsDevice = nullptr;
    PFN_xrCreateVulkanDeviceKHR createVulkanDevice = nullptr;
    if (!result(out, "xrGetInstanceProcAddr(xrCreateVulkanInstanceKHR)",
                xrGetInstanceProcAddr(instance, "xrCreateVulkanInstanceKHR",
                    reinterpret_cast<PFN_xrVoidFunction*>(&createVulkanInstance))) || !createVulkanInstance ||
        !result(out, "xrGetInstanceProcAddr(xrGetVulkanGraphicsDevice2KHR)",
                xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsDevice2KHR",
                    reinterpret_cast<PFN_xrVoidFunction*>(&getGraphicsDevice))) || !getGraphicsDevice ||
        !result(out, "xrGetInstanceProcAddr(xrCreateVulkanDeviceKHR)",
                xrGetInstanceProcAddr(instance, "xrCreateVulkanDeviceKHR",
                    reinterpret_cast<PFN_xrVoidFunction*>(&createVulkanDevice))) || !createVulkanDevice) return;

    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "Perimeter XR Probe";
    app.apiVersion = VK_API_VERSION_1_3;
    VkInstanceCreateInfo vkInstanceInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    vkInstanceInfo.pApplicationInfo = &app;
    XrVulkanInstanceCreateInfoKHR xrInstanceInfo{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
    xrInstanceInfo.systemId = system;
    xrInstanceInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    xrInstanceInfo.vulkanCreateInfo = &vkInstanceInfo;
    VkInstance vkInstance = VK_NULL_HANDLE;
    VkResult vkResult = VK_SUCCESS;
    const XrResult xrResult = createVulkanInstance(instance, &xrInstanceInfo, &vkInstance, &vkResult);
    out << "Vulkan 1.3 instance creation: XrResult=" << static_cast<int>(xrResult)
        << " VkResult=" << static_cast<int>(vkResult) << '\n';
    if (XR_FAILED(xrResult) || vkResult != VK_SUCCESS || !vkInstance) return;

    XrVulkanGraphicsDeviceGetInfoKHR getInfo{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
    getInfo.systemId = system;
    getInfo.vulkanInstance = vkInstance;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    if (result(out, "xrGetVulkanGraphicsDevice2KHR",
               getGraphicsDevice(instance, &getInfo, &physicalDevice)) && physicalDevice) {
        VkPhysicalDeviceProperties gpu{};
        vkGetPhysicalDeviceProperties(physicalDevice, &gpu);
        out << "XR-selected GPU: " << gpu.deviceName << " Vulkan="
            << VK_VERSION_MAJOR(gpu.apiVersion) << '.' << VK_VERSION_MINOR(gpu.apiVersion)
            << '.' << VK_VERSION_PATCH(gpu.apiVersion) << '\n';

        uint32_t queueCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, nullptr);
        std::vector<VkQueueFamilyProperties> queues(queueCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queues.data());
        uint32_t graphicsQueue = queueCount;
        for (uint32_t i = 0; i < queueCount; ++i) {
            if (queues[i].queueCount && (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                graphicsQueue = i;
                break;
            }
        }
        if (graphicsQueue == queueCount) {
            out << "FAIL no Vulkan graphics queue\n";
        } else {
            const float priority = 1.0f;
            VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queueInfo.queueFamilyIndex = graphicsQueue;
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = &priority;
            VkDeviceCreateInfo vkDeviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
            vkDeviceInfo.queueCreateInfoCount = 1;
            vkDeviceInfo.pQueueCreateInfos = &queueInfo;
            XrVulkanDeviceCreateInfoKHR xrDeviceInfo{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
            xrDeviceInfo.systemId = system;
            xrDeviceInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
            xrDeviceInfo.vulkanPhysicalDevice = physicalDevice;
            xrDeviceInfo.vulkanCreateInfo = &vkDeviceInfo;
            VkDevice vkDevice = VK_NULL_HANDLE;
            vkResult = VK_SUCCESS;
            const XrResult deviceResult = createVulkanDevice(instance, &xrDeviceInfo, &vkDevice, &vkResult);
            out << "Vulkan device creation: XrResult=" << static_cast<int>(deviceResult)
                << " VkResult=" << static_cast<int>(vkResult) << '\n';
            if (XR_SUCCEEDED(deviceResult) && vkResult == VK_SUCCESS && vkDevice) {
                XrGraphicsBindingVulkan2KHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
                binding.instance = vkInstance;
                binding.physicalDevice = physicalDevice;
                binding.device = vkDevice;
                binding.queueFamilyIndex = graphicsQueue;
                binding.queueIndex = 0;
                XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO};
                sessionInfo.next = &binding;
                sessionInfo.systemId = system;
                XrSession session = XR_NULL_HANDLE;
                if (result(out, "xrCreateSession", xrCreateSession(instance, &sessionInfo, &session))) {
                    out << "XR Vulkan graphics session created\n";
                    uint32_t formatCount = 0;
                    if (result(out, "xrEnumerateSwapchainFormats(count)",
                               xrEnumerateSwapchainFormats(session, 0, &formatCount, nullptr))) {
                        std::vector<int64_t> formats(formatCount);
                        if (result(out, "xrEnumerateSwapchainFormats(data)",
                                   xrEnumerateSwapchainFormats(session, formatCount, &formatCount,
                                                               formats.data()))) {
                            out << "Swapchain VkFormats:";
                            for (const auto format : formats) out << ' ' << format;
                            out << '\n';
                        }
                    }
                    result(out, "xrDestroySession", xrDestroySession(session));
                }
                vkDestroyDevice(vkDevice, nullptr);
            }
        }
    }
    vkDestroyInstance(vkInstance, nullptr);
}

std::string collect(JNIEnv* env, jobject activity) {
    std::ostringstream out;
    out << "Perimeter XR capability probe\n";
    JavaVM* vm = nullptr;
    if (env->GetJavaVM(&vm) != JNI_OK || !vm) {
        out << "FAIL GetJavaVM\n";
        return out.str();
    }

    PFN_xrInitializeLoaderKHR initialize = nullptr;
    if (!result(out, "xrGetInstanceProcAddr(xrInitializeLoaderKHR)",
                xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                    reinterpret_cast<PFN_xrVoidFunction*>(&initialize))) || !initialize) {
        out << "FAIL Android loader initialization unavailable\n";
        return out.str();
    }
    XrLoaderInitInfoAndroidKHR loaderInfo{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
    loaderInfo.applicationVM = vm;
    loaderInfo.applicationContext = activity;
    if (!result(out, "xrInitializeLoaderKHR", initialize(
            reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&loaderInfo)))) return out.str();

    uint32_t extensionCount = 0;
    if (!result(out, "xrEnumerateInstanceExtensionProperties(count)",
                xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr))) return out.str();
    std::vector<XrExtensionProperties> extensions(extensionCount);
    for (auto& extension : extensions) extension.type = XR_TYPE_EXTENSION_PROPERTIES;
    if (!result(out, "xrEnumerateInstanceExtensionProperties(data)",
                xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount,
                                                        extensions.data()))) return out.str();
    bool hasAndroid = false;
    bool hasVulkan2 = false;
    out << "Runtime extensions:\n";
    for (const auto& extension : extensions) {
        out << "  " << extension.extensionName << " rev " << extension.extensionVersion << '\n';
        hasAndroid |= std::string(extension.extensionName) == XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME;
        hasVulkan2 |= std::string(extension.extensionName) == XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME;
    }
    if (!hasAndroid || !hasVulkan2) {
        out << "FAIL required extensions: android_create_instance=" << hasAndroid
            << " vulkan_enable2=" << hasVulkan2 << '\n';
        return out.str();
    }

    const char* enabled[] = {XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
                             XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME};
    XrInstanceCreateInfoAndroidKHR androidInfo{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
    androidInfo.applicationVM = vm;
    androidInfo.applicationActivity = activity;
    XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    createInfo.next = &androidInfo;
    std::snprintf(createInfo.applicationInfo.applicationName,
                  sizeof(createInfo.applicationInfo.applicationName), "Perimeter XR Probe");
    createInfo.applicationInfo.applicationVersion = 1;
    std::snprintf(createInfo.applicationInfo.engineName,
                  sizeof(createInfo.applicationInfo.engineName), "Perimeter");
    createInfo.applicationInfo.engineVersion = 1;
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    createInfo.enabledExtensionCount = 2;
    createInfo.enabledExtensionNames = enabled;
    XrInstance instance = XR_NULL_HANDLE;
    if (!result(out, "xrCreateInstance", xrCreateInstance(&createInfo, &instance))) return out.str();

    XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};
    if (result(out, "xrGetInstanceProperties", xrGetInstanceProperties(instance, &properties))) {
        out << "Runtime: " << properties.runtimeName << " ";
        version(out, properties.runtimeVersion);
        out << '\n';
    }
    XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId system = XR_NULL_SYSTEM_ID;
    if (result(out, "xrGetSystem", xrGetSystem(instance, &systemInfo, &system))) {
        XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES};
        if (result(out, "xrGetSystemProperties", xrGetSystemProperties(instance, system,
                                                                       &systemProperties))) {
            out << "System: " << systemProperties.systemName << " vendor="
                << systemProperties.vendorId << " orientation="
                << systemProperties.trackingProperties.orientationTracking << " position="
                << systemProperties.trackingProperties.positionTracking << '\n';
        }

        PFN_xrGetVulkanGraphicsRequirements2KHR getRequirements = nullptr;
        if (result(out, "xrGetInstanceProcAddr(xrGetVulkanGraphicsRequirements2KHR)",
                   xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirements2KHR",
                       reinterpret_cast<PFN_xrVoidFunction*>(&getRequirements))) && getRequirements) {
            XrGraphicsRequirementsVulkan2KHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
            if (result(out, "xrGetVulkanGraphicsRequirements2KHR",
                       getRequirements(instance, system, &requirements))) {
                out << "Vulkan API min=";
                version(out, requirements.minApiVersionSupported);
                out << " max-tested=";
                version(out, requirements.maxApiVersionSupported);
                out << '\n';
            }
        }

        probeGraphics(out, instance, system);

        uint32_t viewCount = 0;
        if (result(out, "xrEnumerateViewConfigurationViews(count)",
                   xrEnumerateViewConfigurationViews(instance, system,
                       XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr))) {
            std::vector<XrViewConfigurationView> views(viewCount);
            for (auto& view : views) view.type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
            if (result(out, "xrEnumerateViewConfigurationViews(data)",
                       xrEnumerateViewConfigurationViews(instance, system,
                           XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, viewCount,
                           &viewCount, views.data()))) {
                out << "Stereo views: " << viewCount << '\n';
                for (uint32_t i = 0; i < viewCount; ++i) {
                    out << "  view " << i << " recommended=" << views[i].recommendedImageRectWidth
                        << 'x' << views[i].recommendedImageRectHeight << " max="
                        << views[i].maxImageRectWidth << 'x' << views[i].maxImageRectHeight
                        << " samples=" << views[i].recommendedSwapchainSampleCount << '\n';
                }
            }
        }
    }
    result(out, "xrDestroyInstance", xrDestroyInstance(instance));
    out << "No swapchain images rendered by this probe.\n";
    return out.str();
}
} // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_com_queststoredb_perimeter_xrsmoke_ProbeActivity_probe(JNIEnv* env, jobject activity) {
    const std::string report = collect(env, activity);
    std::printf("%s", report.c_str());
    return env->NewStringUTF(report.c_str());
}
