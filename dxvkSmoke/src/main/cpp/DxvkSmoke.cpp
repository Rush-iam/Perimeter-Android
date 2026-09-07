#include <SDL.h>
#include <SDL_vulkan.h>
#include <android/log.h>
#include <vulkan/vulkan.h>
#include <d3d9.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <stdexcept>
#include <vector>

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

extern "C" int SDL_main(int, char**) {
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
    log("START DXVK c3dd74be6baec53786d4e064a572185b70347a17, arm64-v8a, 300 presents required");
    try {
        require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0, "SDL_Init");
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
    } catch (const std::exception& error) {
        log("FAIL: %s; SDL: %s", error.what(), SDL_GetError());
    } catch (...) {
        // DXVK's DxvkError does not derive from std::exception. Its detailed
        // message is emitted to *_d3d9.log / AndroidLog before crossing the API.
        log("FAIL: DXVK threw a native exception; inspect files/*_d3d9.log and Perimeter logcat for the missing capability");
    }
    if (device) device->Release();
    if (d3d) d3d->Release();
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    if (report) fclose(report);
    return exitCode;
}
