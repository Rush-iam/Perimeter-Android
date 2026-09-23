#if defined(PERIMETER_DXVK_XR)
#include <d3d9_interfaces.h>
#endif
#include <SDL.h>
#include <SDL_vulkan.h>
#include <android/log.h>
#include <vulkan/vulkan.h>
#include <d3d9.h>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <stdexcept>
#include <vector>

#if defined(PERIMETER_DXVK_XR)
#define XR_USE_PLATFORM_ANDROID
#define XR_USE_GRAPHICS_API_VULKAN
#include <jni.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <dxvk_android_xr.h>
#endif

extern "C" void dxvkSetSdl2Window(SDL_Window*) __attribute__((weak));

namespace {
FILE* report = nullptr;

void log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char line[2048];
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    __android_log_write(ANDROID_LOG_INFO, "DxvkSmoke", line);
    if (report) {
        fprintf(report, "%s\n", line);
        fflush(report);
    }
}

void require(bool success, const char* stage) {
    if (!success) throw std::runtime_error(stage);
}

void check(HRESULT result, const char* stage) {
    log("%s: HRESULT=0x%08x", stage, static_cast<unsigned>(result));
    require(SUCCEEDED(result), stage);
}

#if defined(PERIMETER_DXVK_XR)
void checkXr(XrResult resultCode, const char* stage) {
    log("%s: XrResult=%d", stage, static_cast<int>(resultCode));
    require(resultCode == XR_SUCCESS, stage);
}

void checkXrFrame(XrResult resultCode, const char* stage) {
    if (resultCode != XR_SUCCESS)
        log("%s: XrResult=%d", stage, static_cast<int>(resultCode));
    if (resultCode == XR_SESSION_LOSS_PENDING || resultCode == XR_ERROR_SESSION_LOST ||
        resultCode == XR_ERROR_INSTANCE_LOST)
        throw std::runtime_error("XR session/instance lost; relaunch required");
    require(resultCode == XR_SUCCESS, stage);
}

enum class XrProbeOutcome { Passed, Interrupted, SessionLost };

struct XrQueueLock {
    ID3D9VkInteropDevice* interop;
    explicit XrQueueLock(ID3D9VkInteropDevice* device) : interop(device) {
        interop->LockSubmissionQueue();
    }
    ~XrQueueLock() { interop->ReleaseSubmissionQueue(); }
};

struct XrBridge {
    struct EyeTarget {
        IDirect3DResource9* resource = nullptr;
        IDirect3DSurface9* surface = nullptr;
        ID3D9VkInteropTexture* interopImage = nullptr;
        VkImage image = VK_NULL_HANDLE;
        VkImageLayout originalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        uint32_t width = 0;
        uint32_t height = 0;
        bool transferLayout = false;
    };
    JNIEnv* env = nullptr;
    jobject activity = nullptr;
    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId system = XR_NULL_SYSTEM_ID;
    XrSession session = XR_NULL_HANDLE;
    XrSpace localSpace = XR_NULL_HANDLE;
    XrSwapchain eyeSwapchains[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE};
    bool eyeAcquired[2] = {false, false};
    bool eyeWaited[2] = {false, false};
    std::vector<XrSwapchainImageVulkan2KHR> swapchainImages[2];
    EyeTarget eyeTargets[2];
    ID3D9VkInteropDevice* interop = nullptr;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkFence copyFence = VK_NULL_HANDLE;
    VkQueryPool copyTimestampPool = VK_NULL_HANDLE;
    uint32_t timestampValidBits = 0;
    float timestampPeriodNs = 0.0f;
    std::vector<double> transferGpuMilliseconds[4]; // pre-copy, copy, restore, total
    VkInstance vkInstance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice vkDevice = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily = VK_QUEUE_FAMILY_IGNORED;
    PFN_xrCreateVulkanInstanceKHR createInstance = nullptr;
    PFN_xrGetVulkanGraphicsDevice2KHR getGraphicsDevice = nullptr;
    PFN_xrCreateVulkanDeviceKHR createDevice = nullptr;

    void init() {
        env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
        require(env != nullptr, "SDL Android JNI environment");
        jobject localActivity = static_cast<jobject>(SDL_AndroidGetActivity());
        require(localActivity != nullptr, "SDL Android activity");
        activity = env->NewGlobalRef(localActivity);
        env->DeleteLocalRef(localActivity);
        require(activity != nullptr, "Global Android activity reference");
        JavaVM* vm = nullptr;
        require(env->GetJavaVM(&vm) == JNI_OK && vm, "GetJavaVM");

        PFN_xrInitializeLoaderKHR initialize = nullptr;
        checkXr(xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
            reinterpret_cast<PFN_xrVoidFunction*>(&initialize)), "xrGetInstanceProcAddr(loader)");
        require(initialize != nullptr, "OpenXR Android loader function");
        XrLoaderInitInfoAndroidKHR loaderInfo{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
        loaderInfo.applicationVM = vm;
        loaderInfo.applicationContext = activity;
        checkXr(initialize(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&loaderInfo)),
                "xrInitializeLoaderKHR");

        const char* extensions[] = {XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,
                                    XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME};
        XrInstanceCreateInfoAndroidKHR androidInfo{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
        androidInfo.applicationVM = vm;
        androidInfo.applicationActivity = activity;
        XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};
        info.next = &androidInfo;
        std::snprintf(info.applicationInfo.applicationName,
                      sizeof(info.applicationInfo.applicationName), "Perimeter DXVK XR Probe");
        std::snprintf(info.applicationInfo.engineName,
                      sizeof(info.applicationInfo.engineName), "Perimeter");
        info.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
        info.enabledExtensionCount = 2;
        info.enabledExtensionNames = extensions;
        checkXr(xrCreateInstance(&info, &instance), "xrCreateInstance");

        XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
        systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        checkXr(xrGetSystem(instance, &systemInfo, &system), "xrGetSystem");
        checkXr(xrGetInstanceProcAddr(instance, "xrCreateVulkanInstanceKHR",
            reinterpret_cast<PFN_xrVoidFunction*>(&createInstance)), "xrGetInstanceProcAddr(create instance)");
        checkXr(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsDevice2KHR",
            reinterpret_cast<PFN_xrVoidFunction*>(&getGraphicsDevice)), "xrGetInstanceProcAddr(graphics device)");
        checkXr(xrGetInstanceProcAddr(instance, "xrCreateVulkanDeviceKHR",
            reinterpret_cast<PFN_xrVoidFunction*>(&createDevice)), "xrGetInstanceProcAddr(create device)");
        require(createInstance && getGraphicsDevice && createDevice, "OpenXR Vulkan entry points");

        PFN_xrGetVulkanGraphicsRequirements2KHR getRequirements = nullptr;
        checkXr(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirements2KHR",
            reinterpret_cast<PFN_xrVoidFunction*>(&getRequirements)),
            "xrGetInstanceProcAddr(graphics requirements)");
        require(getRequirements != nullptr, "OpenXR Vulkan graphics requirements function");
        XrGraphicsRequirementsVulkan2KHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
        checkXr(getRequirements(instance, system, &requirements),
                "xrGetVulkanGraphicsRequirements2KHR");
        log("XR Vulkan API min=%u.%u max-tested=%u.%u",
            XR_VERSION_MAJOR(requirements.minApiVersionSupported),
            XR_VERSION_MINOR(requirements.minApiVersionSupported),
            XR_VERSION_MAJOR(requirements.maxApiVersionSupported),
            XR_VERSION_MINOR(requirements.maxApiVersionSupported));

        DxvkAndroidXrHooks hooks{};
        hooks.version = DXVK_ANDROID_XR_HOOKS_VERSION;
        hooks.size = sizeof(hooks);
        hooks.userData = this;
        hooks.createInstance = &createVkInstance;
        hooks.selectPhysicalDevice = &selectVkDevice;
        hooks.createDevice = &createVkDevice;
        require(dxvkSetAndroidXrHooks(&hooks), "DXVK XR hook registration");
        log("XR hooks registered before Direct3DCreate9");
    }

    static VkResult createVkInstance(void* user, const VkInstanceCreateInfo* info,
                                    const VkAllocationCallbacks* allocator, VkInstance* output) {
        auto& self = *static_cast<XrBridge*>(user);
        XrVulkanInstanceCreateInfoKHR xrInfo{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
        xrInfo.systemId = self.system;
        xrInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
        xrInfo.vulkanCreateInfo = info;
        xrInfo.vulkanAllocator = allocator;
        VkResult vkResult = VK_ERROR_INITIALIZATION_FAILED;
        XrResult xrResult = self.createInstance(self.instance, &xrInfo, output, &vkResult);
        log("DXVK XR instance: XrResult=%d VkResult=%d", static_cast<int>(xrResult), static_cast<int>(vkResult));
        if (XR_SUCCEEDED(xrResult) && vkResult == VK_SUCCESS) self.vkInstance = *output;
        return XR_SUCCEEDED(xrResult) ? vkResult : VK_ERROR_INITIALIZATION_FAILED;
    }

    static VkResult selectVkDevice(void* user, VkInstance instance, VkPhysicalDevice* output) {
        auto& self = *static_cast<XrBridge*>(user);
        XrVulkanGraphicsDeviceGetInfoKHR info{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
        info.systemId = self.system;
        info.vulkanInstance = instance;
        XrResult resultCode = self.getGraphicsDevice(self.instance, &info, output);
        log("DXVK XR physical device: XrResult=%d", static_cast<int>(resultCode));
        if (XR_SUCCEEDED(resultCode)) self.physicalDevice = *output;
        return XR_SUCCEEDED(resultCode) ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
    }

    static VkResult createVkDevice(void* user, VkPhysicalDevice physical,
                                  const VkDeviceCreateInfo* info,
                                  const VkAllocationCallbacks* allocator, VkDevice* output) {
        auto& self = *static_cast<XrBridge*>(user);
        XrVulkanDeviceCreateInfoKHR xrInfo{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
        xrInfo.systemId = self.system;
        xrInfo.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
        xrInfo.vulkanPhysicalDevice = physical;
        xrInfo.vulkanCreateInfo = info;
        xrInfo.vulkanAllocator = allocator;
        VkResult vkResult = VK_ERROR_INITIALIZATION_FAILED;
        XrResult xrResult = self.createDevice(self.instance, &xrInfo, output, &vkResult);
        log("DXVK XR device: XrResult=%d VkResult=%d", static_cast<int>(xrResult), static_cast<int>(vkResult));
        if (XR_SUCCEEDED(xrResult) && vkResult == VK_SUCCESS) {
            self.vkDevice = *output;
            uint32_t count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
            std::vector<VkQueueFamilyProperties> queues(count);
            vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, queues.data());
            for (uint32_t i = 0; i < info->queueCreateInfoCount; ++i) {
                const uint32_t family = info->pQueueCreateInfos[i].queueFamilyIndex;
                if (family < count && queues[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    self.graphicsQueueFamily = family;
                    break;
                }
            }
        }
        return XR_SUCCEEDED(xrResult) ? vkResult : VK_ERROR_INITIALIZATION_FAILED;
    }

    void createXrSession() {
        require(vkInstance && physicalDevice && vkDevice &&
                graphicsQueueFamily != VK_QUEUE_FAMILY_IGNORED, "DXVK XR Vulkan handles");
        XrGraphicsBindingVulkan2KHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
        binding.instance = vkInstance;
        binding.physicalDevice = physicalDevice;
        binding.device = vkDevice;
        binding.queueFamilyIndex = graphicsQueueFamily;
        binding.queueIndex = 0;
        XrSessionCreateInfo info{XR_TYPE_SESSION_CREATE_INFO};
        info.next = &binding;
        info.systemId = system;
        checkXr(xrCreateSession(instance, &info, &session), "xrCreateSession(shared DXVK device)");
        XrReferenceSpaceCreateInfo spaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        spaceInfo.poseInReferenceSpace.orientation.w = 1.0f;
        checkXr(xrCreateReferenceSpace(session, &spaceInfo, &localSpace),
                "xrCreateReferenceSpace(LOCAL)");
        log("PASS: OpenXR session uses DXVK-created Vulkan instance and device");
    }

    void probeEyeFormat(IDirect3DDevice9* d3dDevice) {
        uint32_t formatCount = 0;
        checkXr(xrEnumerateSwapchainFormats(session, 0, &formatCount, nullptr),
                "xrEnumerateSwapchainFormats(count)");
        std::vector<int64_t> formats(formatCount);
        checkXr(xrEnumerateSwapchainFormats(session, formatCount, &formatCount,
                                           formats.data()), "xrEnumerateSwapchainFormats(data)");
        uint32_t viewConfigCount = 0;
        checkXr(xrEnumerateViewConfigurationViews(instance, system,
                XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewConfigCount, nullptr),
                "xrEnumerateViewConfigurationViews(count)");
        require(viewConfigCount == 2, "OpenXR recommended stereo eye count");
        XrViewConfigurationView viewConfigs[2] = {
            {XR_TYPE_VIEW_CONFIGURATION_VIEW}, {XR_TYPE_VIEW_CONFIGURATION_VIEW}};
        checkXr(xrEnumerateViewConfigurationViews(instance, system,
                XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2, &viewConfigCount, viewConfigs),
                "xrEnumerateViewConfigurationViews(data)");

        check(d3dDevice->QueryInterface(__uuidof(ID3D9VkInteropDevice),
                                      reinterpret_cast<void**>(&interop)), "QueryInterface(D3D9 Vulkan device)");
        uint32_t queueIndex = UINT32_MAX;
        uint32_t queueFamily = UINT32_MAX;
        interop->GetSubmissionQueue(&graphicsQueue, &queueIndex, &queueFamily);
        require(graphicsQueue != VK_NULL_HANDLE && queueIndex == 0 &&
                queueFamily == graphicsQueueFamily, "DXVK/OpenXR shared graphics queue");
        D3D9VkExtImageDesc desc{};
        desc.Type = D3DRTYPE_TEXTURE;
        desc.Depth = 1;
        desc.MipLevels = 1;
        desc.Usage = D3DUSAGE_RENDERTARGET;
        desc.Format = D3DFMT_A8B8G8R8;
        desc.Pool = D3DPOOL_DEFAULT;
        desc.MultiSample = D3DMULTISAMPLE_NONE;
        desc.ImageUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        IDirect3DSurface9* previous = nullptr;
        check(d3dDevice->GetRenderTarget(0, &previous), "GetRenderTarget(previous)");
        for (unsigned eye = 0; eye < 2; ++eye) {
            auto& target = eyeTargets[eye];
            target.width = viewConfigs[eye].recommendedImageRectWidth;
            target.height = viewConfigs[eye].recommendedImageRectHeight;
            require(target.width > 0 && target.height > 0, "OpenXR recommended eye extent");
            desc.Width = target.width;
            desc.Height = target.height;
            check(interop->CreateImage(&desc, &target.resource), "CreateImage(D3D9 XR eye target)");
            auto* texture = static_cast<IDirect3DTexture9*>(target.resource);
            check(texture->GetSurfaceLevel(0, &target.surface), "GetSurfaceLevel(XR eye target)");
            check(d3dDevice->SetRenderTarget(0, target.surface), "SetRenderTarget(XR eye target)");
            const D3DCOLOR color = eye == 0 ? D3DCOLOR_ARGB(255, 220, 35, 35)
                                           : D3DCOLOR_ARGB(255, 35, 90, 220);
            check(d3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET, color, 1.0f, 0),
                  "Clear(XR eye target)");
            check(target.resource->QueryInterface(__uuidof(ID3D9VkInteropTexture),
                reinterpret_cast<void**>(&target.interopImage)),
                "QueryInterface(D3D9 Vulkan eye image)");
        }
        check(d3dDevice->SetRenderTarget(0, previous), "RestoreRenderTarget");
        previous->Release();
        interop->FlushRenderingCommands();
        VkFormat eyeFormat = VK_FORMAT_UNDEFINED;
        for (unsigned eye = 0; eye < 2; ++eye) {
            auto& target = eyeTargets[eye];
            require(interop->WaitForResource(target.resource, 0), "WaitForResource(XR eye target)");
            VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            check(target.interopImage->GetVulkanImageInfo(&target.image,
                &target.originalLayout, &imageInfo), "GetVulkanImageInfo");
            require(target.image != VK_NULL_HANDLE &&
                    target.originalLayout != VK_IMAGE_LAYOUT_UNDEFINED,
                    "D3D9 XR eye Vulkan image and layout");
            require(imageInfo.extent.width == target.width &&
                    imageInfo.extent.height == target.height &&
                    (imageInfo.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
                    "D3D9 XR eye extent and transfer usage");
            require(std::find(formats.begin(), formats.end(),
                    static_cast<int64_t>(imageInfo.format)) != formats.end(),
                    "D3D9 eye format supported by OpenXR");
            if (eye == 0) eyeFormat = imageInfo.format;
            require(imageInfo.format == eyeFormat, "Matching D3D9 XR eye formats");
            log("D3D9 eye %u: VkImage=%p format=%d extent=%ux%u usage=0x%x layout=%d",
                eye, reinterpret_cast<void*>(target.image), static_cast<int>(imageInfo.format),
                target.width, target.height, imageInfo.usage,
                static_cast<int>(target.originalLayout));
        }
        XrSwapchainCreateInfo swapchainInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT |
                                   XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT;
        swapchainInfo.format = static_cast<int64_t>(eyeFormat);
        swapchainInfo.sampleCount = 1;
        swapchainInfo.faceCount = 1;
        swapchainInfo.arraySize = 1;
        swapchainInfo.mipCount = 1;
        for (unsigned eye = 0; eye < 2; ++eye) {
            swapchainInfo.width = eyeTargets[eye].width;
            swapchainInfo.height = eyeTargets[eye].height;
            checkXr(xrCreateSwapchain(session, &swapchainInfo, &eyeSwapchains[eye]),
                    "xrCreateSwapchain(eye transfer destination)");
            uint32_t imageCount = 0;
            checkXr(xrEnumerateSwapchainImages(eyeSwapchains[eye], 0, &imageCount, nullptr),
                    "xrEnumerateSwapchainImages(count)");
            require(imageCount > 0, "OpenXR eye swapchain images");
            auto& images = swapchainImages[eye];
            images.resize(imageCount);
            for (auto& swapchainImage : images)
                swapchainImage.type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR;
            checkXr(xrEnumerateSwapchainImages(eyeSwapchains[eye], imageCount, &imageCount,
                reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())),
                "xrEnumerateSwapchainImages(Vulkan)");
            for (const auto& swapchainImage : images)
                require(swapchainImage.image != VK_NULL_HANDLE, "OpenXR Vulkan eye image");
            log("XR eye %u swapchain: %ux%u VkFormat=%d images=%u usage=0x%llx",
                eye, swapchainInfo.width, swapchainInfo.height,
                static_cast<int>(eyeFormat), imageCount,
                static_cast<unsigned long long>(swapchainInfo.usageFlags));
        }
        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsQueueFamily;
        require(vkCreateCommandPool(vkDevice, &poolInfo, nullptr, &commandPool) == VK_SUCCESS,
                "Vulkan XR copy command pool");
        VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocation.commandPool = commandPool;
        allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocation.commandBufferCount = 1;
        require(vkAllocateCommandBuffers(vkDevice, &allocation, &commandBuffer) == VK_SUCCESS,
                "Vulkan XR copy command buffer");
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        require(vkCreateFence(vkDevice, &fenceInfo, nullptr, &copyFence) == VK_SUCCESS,
                "Vulkan XR copy fence");
        uint32_t familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, families.data());
        require(graphicsQueueFamily < familyCount, "XR timestamp queue family");
        timestampValidBits = families[graphicsQueueFamily].timestampValidBits;
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        timestampPeriodNs = properties.limits.timestampPeriod;
        log("XR copy GPU timestamps: validBits=%u periodNs=%.3f",
            timestampValidBits, timestampPeriodNs);
        if (timestampValidBits) {
            VkQueryPoolCreateInfo queryInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
            queryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
            queryInfo.queryCount = 4;
            require(vkCreateQueryPool(vkDevice, &queryInfo, nullptr, &copyTimestampPool) == VK_SUCCESS,
                    "Vulkan XR copy timestamp pool");
        }
        const VkImageSubresourceRange colorRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        for (auto& target : eyeTargets) {
            interop->TransitionTextureLayout(target.interopImage, &colorRange,
                target.originalLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        }
        interop->FlushRenderingCommands();
        for (auto& target : eyeTargets) {
            require(interop->WaitForResource(target.resource, 0),
                    "WaitForResource(XR transfer-source transition)");
            target.transferLayout = true;
        }
    }

    void copyEyes(const uint32_t imageIndexes[2]) {
        require(vkResetCommandBuffer(commandBuffer, 0) == VK_SUCCESS,
                "Reset XR copy command buffer");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        require(vkBeginCommandBuffer(commandBuffer, &begin) == VK_SUCCESS,
                "Begin XR copy command buffer");
        if (copyTimestampPool) {
            vkCmdResetQueryPool(commandBuffer, copyTimestampPool, 0, 4);
            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                copyTimestampPool, 0);
        }
        for (unsigned eye = 0; eye < 2; ++eye) {
            require(imageIndexes[eye] < swapchainImages[eye].size(), "XR image index in range");
            const VkImage destination = swapchainImages[eye][imageIndexes[eye]].image;
            VkImageMemoryBarrier before[2] = {};
            before[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            before[0].srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            before[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            before[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            before[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            before[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            before[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            before[0].image = eyeTargets[eye].image;
            before[0].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            before[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            before[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            before[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            before[1].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            before[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            before[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            before[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            before[1].image = destination;
            before[1].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, before);
        }
        if (copyTimestampPool)
            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                copyTimestampPool, 1);
        for (unsigned eye = 0; eye < 2; ++eye) {
            const VkImage destination = swapchainImages[eye][imageIndexes[eye]].image;
            VkImageCopy copy{};
            copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.extent = {eyeTargets[eye].width, eyeTargets[eye].height, 1};
            vkCmdCopyImage(commandBuffer, eyeTargets[eye].image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destination,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        }
        if (copyTimestampPool)
            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                copyTimestampPool, 2);
        for (unsigned eye = 0; eye < 2; ++eye) {
            const VkImage destination = swapchainImages[eye][imageIndexes[eye]].image;
            VkImageMemoryBarrier after{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            after.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            after.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            after.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            after.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            after.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            after.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            after.image = destination;
            after.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &after);
        }
        if (copyTimestampPool)
            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                copyTimestampPool, 3);
        require(vkEndCommandBuffer(commandBuffer) == VK_SUCCESS, "End XR copy command buffer");
        require(vkResetFences(vkDevice, 1, &copyFence) == VK_SUCCESS, "Reset XR copy fence");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &commandBuffer;
        {
            XrQueueLock queue(interop);
            require(vkQueueSubmit(graphicsQueue, 1, &submit, copyFence) == VK_SUCCESS,
                    "Submit XR eye copies");
        }
        require(vkWaitForFences(vkDevice, 1, &copyFence, VK_TRUE, 1000000000) == VK_SUCCESS,
                "Wait for XR eye copies");
        if (copyTimestampPool) {
            uint64_t ticks[4] = {};
            require(vkGetQueryPoolResults(vkDevice, copyTimestampPool, 0, 4,
                sizeof(ticks), ticks, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT) == VK_SUCCESS,
                "XR copy GPU timestamp results");
            const uint64_t mask = timestampValidBits == 64 ? UINT64_MAX
                : (uint64_t{1} << timestampValidBits) - 1;
            const unsigned from[4] = {0, 1, 2, 0};
            const unsigned to[4] = {1, 2, 3, 3};
            for (unsigned section = 0; section < 4; ++section) {
                const uint64_t elapsedTicks = (ticks[to[section]] - ticks[from[section]]) & mask;
                transferGpuMilliseconds[section].push_back(
                    static_cast<double>(elapsedTicks) * timestampPeriodNs / 1000000.0);
            }
        }
    }

    XrProbeOutcome runFrameProbe(unsigned durationSeconds, bool injectFailureAfterEyeWait,
                                unsigned simulateSessionLossAfterFrames) {
        require(interop != nullptr && eyeSwapchains[0] && eyeSwapchains[1],
                "XR frame probe resources");
        bool running = false;
        bool requestedExit = false;
        bool androidBackgrounded = false;
        unsigned frameCount = 0;
        unsigned acquiredFrames = 0;
        unsigned pauseCount = 0;
        Uint64 renderUntil = 0;
        Uint64 pausedAt = 0;
        const Uint64 deadline = SDL_GetTicks64() +
            static_cast<Uint64>(durationSeconds + 50) * 1000;
        while (SDL_GetTicks64() < deadline) {
            if (simulateSessionLossAfterFrames && frameCount >= simulateSessionLossAfterFrames) {
                log("SIMULATED LOSS: after %u frames; destroying session without xrEndSession",
                    frameCount);
                return XrProbeOutcome::SessionLost;
            }
            XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
            XrResult pollResult = XR_SUCCESS;
            while ((pollResult = xrPollEvent(instance, &event)) == XR_SUCCESS) {
                if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
                    log("LOSS: OpenXR instance loss pending; relaunch required");
                    return XrProbeOutcome::SessionLost;
                }
                if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                    const auto& change = *reinterpret_cast<const XrEventDataSessionStateChanged*>(&event);
                    if (change.session == session) {
                        log("XR session state=%d", static_cast<int>(change.state));
                        if (change.state == XR_SESSION_STATE_READY && !running) {
                            XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};
                            begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                            checkXr(xrBeginSession(session, &begin), "xrBeginSession");
                            running = true;
                            androidBackgrounded = false;
                            if (renderUntil == 0)
                                renderUntil = SDL_GetTicks64() +
                                    static_cast<Uint64>(durationSeconds) * 1000;
                            else if (pausedAt) renderUntil += SDL_GetTicks64() - pausedAt;
                            pausedAt = 0;
                            log("XR session ready: pauses=%u", pauseCount);
                        } else if (change.state == XR_SESSION_STATE_STOPPING && running) {
                            checkXr(xrEndSession(session), "xrEndSession");
                            running = false;
                            if (requestedExit) {
                                require(frameCount > 0 && acquiredFrames > 0,
                                        "XR frame probe completed before session stop");
                                log("PASS: %u paced XR frames, %u copied stereo projection layers, %u pauses",
                                    frameCount, acquiredFrames, pauseCount);
                                if (!transferGpuMilliseconds[0].empty()) {
                                const size_t warmup = std::min<size_t>(60,
                                    transferGpuMilliseconds[0].size() / 2);
                                const char* sectionNames[4] = {
                                    "pre-copy barriers", "two image copies", "restore barriers", "total"};
                                for (unsigned section = 0; section < 4; ++section) {
                                    std::vector<double> measured(
                                        transferGpuMilliseconds[section].begin() + warmup,
                                        transferGpuMilliseconds[section].end());
                                    double total = 0.0;
                                    for (double milliseconds : measured) total += milliseconds;
                                    std::sort(measured.begin(), measured.end());
                                    const size_t p50 = (measured.size() * 50 + 99) / 100 - 1;
                                    const size_t p95 = (measured.size() * 95 + 99) / 100 - 1;
                                    log("XR GPU transfer %ux%u + %ux%u %s: samples=%zu warmup=%zu mean=%.4fms p50=%.4fms p95=%.4fms max=%.4fms",
                                        eyeTargets[0].width, eyeTargets[0].height,
                                        eyeTargets[1].width, eyeTargets[1].height,
                                        sectionNames[section], measured.size(), warmup,
                                        total / measured.size(), measured[p50],
                                        measured[p95], measured.back());
                                }
                                }
                                return XrProbeOutcome::Passed;
                            }
                            ++pauseCount;
                            pausedAt = SDL_GetTicks64();
                            log("XR session stopped before probe completion; waiting for READY");
                        } else if (change.state == XR_SESSION_STATE_LOSS_PENDING) {
                            log("LOSS: OpenXR session loss pending; relaunch required");
                            return XrProbeOutcome::SessionLost;
                        } else if (change.state == XR_SESSION_STATE_EXITING) {
                            log("INTERRUPTED: OpenXR session exiting");
                            return XrProbeOutcome::Interrupted;
                        }
                    }
                }
                event = {XR_TYPE_EVENT_DATA_BUFFER};
            }
            checkXrFrame(pollResult == XR_EVENT_UNAVAILABLE ? XR_SUCCESS : pollResult,
                "xrPollEvent");
            SDL_Event sdlEvent;
            while (SDL_PollEvent(&sdlEvent)) {
                if (sdlEvent.type == SDL_QUIT) {
                    if (pauseCount || androidBackgrounded) {
                        log("INTERRUPTED: SDLActivity quit after XR background/stop; relaunch required");
                        return XrProbeOutcome::Interrupted;
                    }
                    throw std::runtime_error("XR frame probe interrupted by unexpected quit");
                }
                if (sdlEvent.type == SDL_APP_WILLENTERBACKGROUND) {
                    androidBackgrounded = true;
                    if (!pausedAt) pausedAt = SDL_GetTicks64();
                    log("SDL application entering background");
                } else if (sdlEvent.type == SDL_APP_DIDENTERFOREGROUND) {
                    androidBackgrounded = false;
                    if (running && pausedAt) {
                        renderUntil += SDL_GetTicks64() - pausedAt;
                        pausedAt = 0;
                    }
                    log("SDL application entered foreground");
                }
            }
            if (!running || androidBackgrounded) {
                SDL_Delay(10);
                continue;
            }
            if (SDL_GetTicks64() >= renderUntil) {
                if (!requestedExit) {
                    checkXr(xrRequestExitSession(session), "xrRequestExitSession");
                    requestedExit = true;
                }
                SDL_Delay(10);
                continue;
            }
            XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
            XrFrameState frameState{XR_TYPE_FRAME_STATE};
            checkXrFrame(xrWaitFrame(session, &waitInfo, &frameState), "xrWaitFrame");
            XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
            {
                XrQueueLock queue(interop);
                checkXrFrame(xrBeginFrame(session, &beginInfo), "xrBeginFrame");
            }
            XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};
            locate.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            locate.displayTime = frameState.predictedDisplayTime;
            locate.space = localSpace;
            XrViewState viewState{XR_TYPE_VIEW_STATE};
            XrView views[2] = {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}};
            uint32_t viewCount = 0;
            checkXrFrame(xrLocateViews(session, &locate, &viewState, 2, &viewCount, views),
                    "xrLocateViews");
            require(viewCount == 2, "OpenXR stereo view count");
            const bool validViews = (viewState.viewStateFlags &
                (XR_VIEW_STATE_ORIENTATION_VALID_BIT | XR_VIEW_STATE_POSITION_VALID_BIT)) ==
                (XR_VIEW_STATE_ORIENTATION_VALID_BIT | XR_VIEW_STATE_POSITION_VALID_BIT);
            XrCompositionLayerProjectionView projectionViews[2] = {
                {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},
                {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}};
            XrCompositionLayerProjection projection{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
            projection.space = localSpace;
            projection.viewCount = 2;
            projection.views = projectionViews;
            const XrCompositionLayerBaseHeader* layers[1] = {
                reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection)};
            if (frameState.shouldRender && validViews) {
                uint32_t imageIndexes[2] = {};
                for (unsigned eye = 0; eye < 2; ++eye) {
                    XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
                    XrResult acquireResult;
                    {
                        XrQueueLock queue(interop);
                        acquireResult = xrAcquireSwapchainImage(
                            eyeSwapchains[eye], &acquire, &imageIndexes[eye]);
                    }
                    if (XR_SUCCEEDED(acquireResult)) eyeAcquired[eye] = true;
                    checkXrFrame(acquireResult, "xrAcquireSwapchainImage");
                    XrSwapchainImageWaitInfo imageWait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                    imageWait.timeout = XR_INFINITE_DURATION;
                    const XrResult imageWaitResult =
                        xrWaitSwapchainImage(eyeSwapchains[eye], &imageWait);
                    if (imageWaitResult == XR_SUCCESS ||
                        imageWaitResult == XR_SESSION_LOSS_PENDING) eyeWaited[eye] = true;
                    checkXrFrame(imageWaitResult, "xrWaitSwapchainImage");
                    if (injectFailureAfterEyeWait && frameCount == 0 && eye == 0)
                        throw std::runtime_error("Injected failure after first XR eye wait");
                    projectionViews[eye].pose = views[eye].pose;
                    projectionViews[eye].fov = views[eye].fov;
                    projectionViews[eye].subImage.swapchain = eyeSwapchains[eye];
                    projectionViews[eye].subImage.imageRect.extent = {
                        static_cast<int32_t>(eyeTargets[eye].width),
                        static_cast<int32_t>(eyeTargets[eye].height)};
                    projectionViews[eye].subImage.imageArrayIndex = 0;
                }
                copyEyes(imageIndexes);
                for (unsigned eye = 0; eye < 2; ++eye) {
                    XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                    XrResult releaseResult;
                    {
                        XrQueueLock queue(interop);
                        releaseResult = xrReleaseSwapchainImage(eyeSwapchains[eye], &release);
                    }
                    if (XR_SUCCEEDED(releaseResult)) {
                        eyeAcquired[eye] = false;
                        eyeWaited[eye] = false;
                    }
                    checkXrFrame(releaseResult, "xrReleaseSwapchainImage");
                    if (frameCount == 0)
                        log("XR eye %u acquired image index=%u", eye, imageIndexes[eye]);
                }
                ++acquiredFrames;
            }
            XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
            endInfo.displayTime = frameState.predictedDisplayTime;
            endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
            if (frameState.shouldRender && validViews) {
                endInfo.layerCount = 1;
                endInfo.layers = layers;
            }
            {
                XrQueueLock queue(interop);
                checkXrFrame(xrEndFrame(session, &endInfo), "xrEndFrame(stereo projection)");
            }
            ++frameCount;
            if (frameCount % 120 == 0)
                log("XR stereo projection progress: frames=%u copied=%u", frameCount, acquiredFrames);
        }
        throw std::runtime_error("XR frame probe timed out waiting for session state");
    }

    void destroySession() {
        bool queueIdle = true;
        if (graphicsQueue && interop) {
            XrQueueLock queue(interop);
            const VkResult resultCode = vkQueueWaitIdle(graphicsQueue);
            log("vkQueueWaitIdle(copy teardown): VkResult=%d", static_cast<int>(resultCode));
            queueIdle = resultCode == VK_SUCCESS;
        }
        // A failure after acquire/wait must not leave a reusable swapchain image
        // leased to this probe. Only release once all submitted GPU work is done.
        if (queueIdle && interop) {
            for (unsigned eye = 0; eye < 2; ++eye) {
                if (!eyeAcquired[eye] || !eyeSwapchains[eye]) continue;
                if (!eyeWaited[eye]) {
                    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                    waitInfo.timeout = 1000000000; // bounded teardown after a failed frame
                    const XrResult waitResult = xrWaitSwapchainImage(eyeSwapchains[eye], &waitInfo);
                    log("xrWaitSwapchainImage(teardown eye %u): XrResult=%d",
                        eye, static_cast<int>(waitResult));
                    eyeWaited[eye] = waitResult == XR_SUCCESS ||
                                     waitResult == XR_SESSION_LOSS_PENDING;
                }
                if (eyeWaited[eye]) {
                    XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                    XrResult releaseResult;
                    {
                        XrQueueLock queue(interop);
                        releaseResult = xrReleaseSwapchainImage(eyeSwapchains[eye], &release);
                    }
                    log("xrReleaseSwapchainImage(teardown eye %u): XrResult=%d",
                        eye, static_cast<int>(releaseResult));
                    if (XR_SUCCEEDED(releaseResult)) eyeAcquired[eye] = false;
                }
            }
        }
        if (copyFence) {
            vkDestroyFence(vkDevice, copyFence, nullptr);
            copyFence = VK_NULL_HANDLE;
        }
        if (copyTimestampPool) {
            vkDestroyQueryPool(vkDevice, copyTimestampPool, nullptr);
            copyTimestampPool = VK_NULL_HANDLE;
        }
        if (commandPool) {
            vkDestroyCommandPool(vkDevice, commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
            commandBuffer = VK_NULL_HANDLE;
        }
        if (interop) {
            const VkImageSubresourceRange colorRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            for (auto& target : eyeTargets) {
                if (target.transferLayout && target.interopImage) {
                    interop->TransitionTextureLayout(target.interopImage, &colorRange,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target.originalLayout);
                    target.transferLayout = false;
                }
            }
            interop->FlushRenderingCommands();
            for (auto& target : eyeTargets) {
                if (target.resource)
                    log("WaitForResource(eye teardown)=%d",
                        interop->WaitForResource(target.resource, 0));
            }
        }
        for (auto& target : eyeTargets) {
            if (target.interopImage) target.interopImage->Release();
            if (target.surface) target.surface->Release();
            if (target.resource) target.resource->Release();
            target = {};
        }
        for (auto& swapchain : eyeSwapchains) {
            if (swapchain) {
                log("xrDestroySwapchain: XrResult=%d", static_cast<int>(xrDestroySwapchain(swapchain)));
                swapchain = XR_NULL_HANDLE;
            }
        }
        if (interop) {
            interop->Release();
            interop = nullptr;
        }
        if (localSpace) {
            log("xrDestroySpace: XrResult=%d", static_cast<int>(xrDestroySpace(localSpace)));
            localSpace = XR_NULL_HANDLE;
        }
        if (session) {
            log("xrDestroySession: XrResult=%d", static_cast<int>(xrDestroySession(session)));
            session = XR_NULL_HANDLE;
        }
    }

    void destroy() {
        if (instance) {
            log("xrDestroyInstance: XrResult=%d", static_cast<int>(xrDestroyInstance(instance)));
            instance = XR_NULL_HANDLE;
        }
        if (activity) env->DeleteGlobalRef(activity);
        activity = nullptr;
    }
};
#endif

// Diagnostic only: DXVK's pinned device creation remains the authoritative
// capability test. Do not silently mask missing features or enable workarounds.
void probeVulkan() {
    uint32_t version = VK_API_VERSION_1_0;
    const auto enumerateVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
        vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
    if (enumerateVersion) require(enumerateVersion(&version) == VK_SUCCESS, "Vulkan loader version");
    log("Vulkan loader: %u.%u.%u", VK_VERSION_MAJOR(version), VK_VERSION_MINOR(version), VK_VERSION_PATCH(version));
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "Perimeter DXVK probe";
    app.apiVersion = version < VK_API_VERSION_1_3 ? version : VK_API_VERSION_1_3;
    VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    info.pApplicationInfo = &app;
    VkInstance instance = VK_NULL_HANDLE;
    require(vkCreateInstance(&info, nullptr, &instance) == VK_SUCCESS, "Vulkan probe instance");
    try {
        uint32_t count = 0;
        require(vkEnumeratePhysicalDevices(instance, &count, nullptr) == VK_SUCCESS && count, "Vulkan physical devices");
        std::vector<VkPhysicalDevice> devices(count);
        require(vkEnumeratePhysicalDevices(instance, &count, devices.data()) == VK_SUCCESS, "Vulkan device enumeration");
        for (auto device : devices) {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(device, &properties);
            log("GPU: %s vendor=0x%x device=0x%x driver=0x%x Vulkan=%u.%u.%u pushConstants=%u",
                properties.deviceName, properties.vendorID, properties.deviceID, properties.driverVersion,
                VK_VERSION_MAJOR(properties.apiVersion), VK_VERSION_MINOR(properties.apiVersion),
                VK_VERSION_PATCH(properties.apiVersion), properties.limits.maxPushConstantsSize);
            if (properties.apiVersion < VK_API_VERSION_1_3)
                log("INCOMPATIBLE: pinned DXVK requires Vulkan 1.3; continuing to record DXVK's own diagnostic");
            uint32_t extensionCount = 0;
            require(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr) == VK_SUCCESS, "Vulkan extension count");
            std::vector<VkExtensionProperties> extensions(extensionCount);
            require(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data()) == VK_SUCCESS, "Vulkan extensions");
            bool robustness2 = false;
            for (const auto& extension : extensions) {
                log("extension: %s", extension.extensionName);
                robustness2 |= !strcmp(extension.extensionName, "VK_EXT_robustness2");
            }
            VkPhysicalDeviceFeatures features{};
            vkGetPhysicalDeviceFeatures(device, &features);
#define FEATURE(name) log("feature: " #name "=%u", features.name)
            FEATURE(robustBufferAccess);
            FEATURE(geometryShader);
            FEATURE(depthClamp);
            FEATURE(dualSrcBlend);
            FEATURE(shaderInt64);
            FEATURE(textureCompressionBC);
            FEATURE(multiDrawIndirect);
            FEATURE(multiViewport);
            FEATURE(shaderClipDistance);
            FEATURE(shaderCullDistance);
#undef FEATURE
            if (properties.apiVersion >= VK_API_VERSION_1_3) {
                VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
                VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
                VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
                VkPhysicalDeviceRobustness2FeaturesEXT robust{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};
                f2.pNext = &f12;
                f12.pNext = &f13;
                if (robustness2) f13.pNext = &robust;
                vkGetPhysicalDeviceFeatures2(device, &f2);
                log("feature: descriptorIndexing=%u runtimeDescriptorArray=%u bufferDeviceAddress=%u timelineSemaphore=%u",
                    f12.descriptorIndexing, f12.runtimeDescriptorArray, f12.bufferDeviceAddress, f12.timelineSemaphore);
                log("feature: dynamicRendering=%u synchronization2=%u maintenance4=%u robustBufferAccess2=%u nullDescriptor=%u",
                    f13.dynamicRendering, f13.synchronization2, f13.maintenance4, robust.robustBufferAccess2, robust.nullDescriptor);
            }
        }
    } catch (...) {
        vkDestroyInstance(instance, nullptr);
        throw;
    }
    vkDestroyInstance(instance, nullptr);
}
}

extern "C" int SDL_main(int argc, char** argv) {
#if defined(PERIMETER_DXVK_XR)
    XrBridge xr;
    unsigned xrProbeSeconds = 10;
    bool injectFailureAfterEyeWait = false;
    unsigned simulateSessionLossAfterFrames = 0;
    for (int i = 0; i < argc; ++i) {
        if (argv[i] && std::strncmp(argv[i], "--xr-seconds=", 13) == 0) {
            const long requested = std::strtol(argv[i] + 13, nullptr, 10);
            if (requested >= 1 && requested <= 60)
                xrProbeSeconds = static_cast<unsigned>(requested);
        }
        if (argv[i] && std::strcmp(argv[i], "--xr-fail-after-eye-wait") == 0)
            injectFailureAfterEyeWait = true;
        if (argv[i] && std::strncmp(argv[i], "--xr-simulate-session-loss-after-frames=", 40) == 0) {
            const long requested = std::strtol(argv[i] + 40, nullptr, 10);
            if (requested >= 1 && requested <= 2000)
                simulateSessionLossAfterFrames = static_cast<unsigned>(requested);
        }
    }
#endif
    SDL_Window* window = nullptr;
    IDirect3D9* d3d = nullptr;
    IDirect3DDevice9* device = nullptr;
    int exitCode = 1;
    const char* storage = SDL_AndroidGetInternalStoragePath();
    if (storage) {
        report = fopen((std::string(storage) + "/dxvk-smoke.txt").c_str(), "w");
        setenv("DXVK_LOG_PATH", storage, 1);
        setenv("DXVK_SHADER_CACHE_PATH", storage, 1);
    }
    setenv("DXVK_WSI_DRIVER", "SDL2", 1);
#if defined(PERIMETER_DXVK_XR)
    log("START DXVK/OpenXR shared-device probe, DXVK c3dd74be6baec53786d4e064a572185b70347a17, duration=%us",
        xrProbeSeconds);
#else
    log("START DXVK c3dd74be6baec53786d4e064a572185b70347a17, arm64-v8a, 300 presents required");
#endif
    try {
        require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0, "SDL_Init");
#if defined(PERIMETER_DXVK_XR)
        xr.init();
#endif
        probeVulkan();
        window = SDL_CreateWindow("DXVK Smoke", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                  640, 360, SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN_DESKTOP);
        require(window != nullptr, "SDL Vulkan window");
        if (dxvkSetSdl2Window)
            dxvkSetSdl2Window(window);
        d3d = Direct3DCreate9(D3D_SDK_VERSION);
        require(d3d != nullptr, "Direct3DCreate9");
        log("D3D9 adapters: %u", d3d->GetAdapterCount());
        require(d3d->GetAdapterCount() > 0, "D3D9 adapter availability");
        D3DCAPS9 caps{};
        check(d3d->GetDeviceCaps(0, D3DDEVTYPE_HAL, &caps), "GetDeviceCaps");
        log("D3D9 shaders: vertex=0x%x pixel=0x%x maxTexture=%ux%u", caps.VertexShaderVersion,
            caps.PixelShaderVersion, caps.MaxTextureWidth, caps.MaxTextureHeight);
        int width = 0, height = 0;
        SDL_Vulkan_GetDrawableSize(window, &width, &height);
        require(width > 0 && height > 0, "Vulkan drawable size");
        D3DPRESENT_PARAMETERS presentation{};
        presentation.BackBufferWidth = width;
        presentation.BackBufferHeight = height;
        presentation.BackBufferFormat = D3DFMT_X8R8G8B8;
        presentation.BackBufferCount = 1;
        presentation.SwapEffect = D3DSWAPEFFECT_DISCARD;
        presentation.hDeviceWindow = static_cast<HWND>(window);
        presentation.Windowed = TRUE;
        presentation.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
        check(d3d->CreateDevice(0, D3DDEVTYPE_HAL, presentation.hDeviceWindow,
            D3DCREATE_HARDWARE_VERTEXPROCESSING, &presentation, &device), "CreateDevice");
#if defined(PERIMETER_DXVK_XR)
        xr.createXrSession();
        xr.probeEyeFormat(device);
        const XrProbeOutcome outcome = xr.runFrameProbe(xrProbeSeconds,
            injectFailureAfterEyeWait, simulateSessionLossAfterFrames);
        if (outcome == XrProbeOutcome::Passed)
            log("PASS: DXVK/OpenXR shared-device smoke gate");
        else if (outcome == XrProbeOutcome::Interrupted)
            log("INCOMPLETE: XR smoke run stopped by Android activity lifecycle");
        else
            log("INCOMPLETE: XR session/instance loss path requires relaunch");
        exitCode = outcome == XrProbeOutcome::SessionLost ? 1 : 0;
#else
        const Uint64 start = SDL_GetTicks64();
        unsigned frames = 0;
        while (frames < 300) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                require(event.type != SDL_QUIT && event.type != SDL_APP_WILLENTERBACKGROUND,
                        "Probe interrupted (not a successful run)");
            }
            require(SDL_GetTicks64() - start < 30000, "Presentation timeout");
            const D3DCOLOR colors[] = {D3DCOLOR_XRGB(190, 40, 40), D3DCOLOR_XRGB(40, 190, 40), D3DCOLOR_XRGB(40, 40, 190)};
            const auto clear = device->Clear(0, nullptr, D3DCLEAR_TARGET, colors[(frames / 100) % 3], 1.0f, 0);
            if (FAILED(clear)) check(clear, "Clear");
            const auto present = device->Present(nullptr, nullptr, nullptr, nullptr);
            if (present != D3D_OK) {
                log("Present: HRESULT=0x%08x", static_cast<unsigned>(present));
                throw std::runtime_error("Present did not return D3D_OK");
            }
            ++frames;
            if (frames % 100 == 0) log("Presented %u/300 frames", frames);
            SDL_Delay(16);
        }
        log("PASS: device creation, clear and 300 presents in %llu ms; visual correctness requires observation",
            static_cast<unsigned long long>(SDL_GetTicks64() - start));
        exitCode = 0;
#endif
    } catch (const std::exception& error) {
        log("FAIL: %s; SDL: %s", error.what(), SDL_GetError());
    } catch (...) {
        // DXVK's DxvkError does not derive from std::exception. Its detailed
        // message is emitted to *_d3d9.log / AndroidLog before crossing the API.
        log("FAIL: DXVK threw a native exception; inspect files/*_d3d9.log and Perimeter logcat for the missing capability");
    }
#if defined(PERIMETER_DXVK_XR)
    xr.destroySession();
#endif
    if (device) device->Release();
    if (d3d) d3d->Release();
#if defined(PERIMETER_DXVK_XR)
    dxvkResetAndroidXrHooks();
    xr.destroy();
#endif
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    if (report) fclose(report);
    return exitCode;
}
