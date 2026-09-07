"""Small, idempotent Android adaptations for the pinned DXVK source only.

Fail on source drift rather than silently applying a partial patch. The Vulkan
loader already supports libvulkan.so; SDL still owns Android surface creation.
"""
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
variant = sys.argv[2] if len(sys.argv) > 2 else "v2"


def replace(file, old, new):
    path = root / file
    text = path.read_text(encoding="utf-8")
    if new in text:
        return
    if text.count(old) != 1:
        raise RuntimeError(f"Unexpected pinned DXVK source in {file}: {old!r}")
    path.write_text(text.replace(old, new), encoding="utf-8", newline="\n")


def replace_if_present(file, old, new):
    path = root / file
    if not path.exists():
        return
    text = path.read_text(encoding="utf-8")
    if new and new in text:
        return
    if old in text:
        path.write_text(text.replace(old, new), encoding="utf-8", newline="\n")


def write_if_missing(file, content):
    path = root / file
    if not path.exists():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8", newline="\n")


if variant == "v2":
    replace("src/wsi/sdl2/wsi_platform_sdl2.cpp",
            '#elif defined(__APPLE__)',
            '#elif defined(__ANDROID__)\n        "libSDL2.so"\n#elif defined(__APPLE__)')
    replace_if_present("meson_options.txt", "option('enable_dxgi',",
                       "option('android_sdl2_include', type: 'string', value: '', description: 'Android SDL2 headers')\noption('enable_dxgi',")
    replace_if_present("meson.build", "find_program('touch')",
                       "find_program('touch', native: true)")
    replace_if_present("meson.build", "find_program('glslang', 'glslangValidator')",
                       "find_program('glslang', 'glslangValidator', native: true)")
    replace_if_present("meson.build",
                       "  lib_sdl2 = dependency('sdl2', required: get_option('native_sdl2'))",
                       """  if platform == 'android'
    # SDL2 WSI resolves functions dynamically; use the app's matching headers.
    assert(get_option('android_sdl2_include') != '', 'Android SDL2 headers required')
    lib_sdl2 = declare_dependency(compile_args: ['-I' + get_option('android_sdl2_include')])
  else
    lib_sdl2 = dependency('sdl2', required: get_option('native_sdl2'))
  endif""")
    replace_if_present("meson.build",
                       "  link_args += [\n    '-static-libgcc',\n    '-static-libstdc++',\n  ]",
                       """  if platform == 'android'
    # APK libraries use unversioned SONAMEs and the application's shared libc++.
    dxvk_so_version = {}
  else
    link_args += ['-static-libgcc', '-static-libstdc++']
  endif""")
else:
    # The native 1.x fork links SDL2 directly and has no dynamic WSI loader.
    replace("src/vulkan/vulkan_presenter.cpp",
            "    swapInfo.compositeAlpha         = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;",
            """    // Prefer opaque, otherwise select a mode actually supported by Android WSI.
    swapInfo.compositeAlpha = static_cast<VkCompositeAlphaFlagBitsKHR>(
      caps.supportedCompositeAlpha & (~caps.supportedCompositeAlpha + 1u));""")
    replace_if_present("src/d3d9/d3d9_swapchain.cpp",
            """    if (status != VK_SUCCESS) {
      Logger::info(str::format(
        "D3D9SwapChainEx: present status before recreate: ", status));
      RecreateSwapChain(m_vsync);
    }""",
            """    // Android WSI may report a persistently suboptimal surface
    // even though presentation remains valid. Recreating for that status
    // every frame causes a destructive swap-chain loop.
    if (status != VK_SUCCESS && status != VK_SUBOPTIMAL_KHR)
      RecreateSwapChain(m_vsync);""")
    replace_if_present("src/d3d9/d3d9_swapchain.cpp",
            "    if (status != VK_SUCCESS)\n      RecreateSwapChain(m_vsync);",
            """    // Android WSI may report a persistently suboptimal surface
    // even though presentation remains valid. Recreating for that status
    // every frame causes a destructive swap-chain loop.
    if (status != VK_SUCCESS && status != VK_SUBOPTIMAL_KHR)
      RecreateSwapChain(m_vsync);""")
    replace("src/dxvk/dxvk_graphics.cpp",
            "    for (uint32_t i = 0; i < state.il.attributeCount(); i++) {\n      viAttribs[i] = state.ilAttributes[i].description();\n      viAttribs[i].binding = viBindingMap[state.ilAttributes[i].binding()];\n    }",
            """    uint32_t viAttributeCount = 0;
    for (uint32_t i = 0; i < state.il.attributeCount(); i++) {
      // D3D9's fixed-function signature includes unused blend attributes at
      // locations 16 and 17; Mali only supports 16 vertex inputs.
      if (!(m_shaders.vs->interfaceSlots().inputSlots & (1u << state.ilAttributes[i].location())))
        continue;
      viAttribs[viAttributeCount] = state.ilAttributes[i].description();
      viAttribs[viAttributeCount++].binding = viBindingMap[state.ilAttributes[i].binding()];
    }""")
    replace("src/dxvk/dxvk_graphics.cpp",
            "viInfo.vertexAttributeDescriptionCount  = state.il.attributeCount();",
            "viInfo.vertexAttributeDescriptionCount  = viAttributeCount;")
    replace("src/dxvk/dxvk_graphics.cpp",
            "    uint32_t sampleMask = state.ms.sampleMask();",
            """    rsInfo.depthClampEnable &= m_pipeMgr->m_device->features().core.features.depthClamp;

    uint32_t sampleMask = state.ms.sampleMask();""")
    replace("src/dxvk/dxvk_renderpass.cpp",
            "    VkRenderPassCreateInfo info;",
            """#if defined(__ANDROID__)
    // Framebuffers and pipelines use the default render pass, while draws use
    // load/store variants. Dependencies must match across compatible passes.
    // Use conservative external barriers on this legacy mobile path; retain
    // the framebuffer-local self dependency needed for attachment sampling.
    subpassDepCount = 3;
    subpassDeps[0] = { VK_SUBPASS_EXTERNAL, 0,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
      VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, 0 };
    subpassDeps[1] = { 0, 0,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
      VK_DEPENDENCY_BY_REGION_BIT };
    subpassDeps[2] = { 0, VK_SUBPASS_EXTERNAL,
      VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
      VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, 0 };
#endif

    VkRenderPassCreateInfo info;""")
    # ClipDistance is optional on mobile. Do not emit that SPIR-V capability
    # (even for disabled planes) when the Vulkan device cannot support it.
    replace("src/dxso/dxso_options.h", "    bool useDemoteToHelperInvocation = false;",
            "    bool useDemoteToHelperInvocation = false;\n    bool useClipDistance = true;")
    replace("src/dxso/dxso_options.cpp",
            "    const DxvkDeviceFeatures& devFeatures = device->features();",
            "    const DxvkDeviceFeatures& devFeatures = device->features();\n    useClipDistance = devFeatures.core.features.shaderClipDistance;")
    replace("src/dxso/dxso_compiler.cpp",
            "    m_module.enableCapability(spv::CapabilityClipDistance);",
            "    if (m_moduleInfo.options.useClipDistance)\n      m_module.enableCapability(spv::CapabilityClipDistance);")
    replace("src/dxso/dxso_compiler.cpp", "  void DxsoCompiler::emitVsClipping() {",
            "  void DxsoCompiler::emitVsClipping() {\n    if (!m_moduleInfo.options.useClipDistance) return;")
    replace("src/d3d9/d3d9_fixed_function.h",
            "D3D9FixedFunctionOptions(const D3D9Options* options);",
            "D3D9FixedFunctionOptions(D3D9DeviceEx* device);\n    bool useClipDistance;")
    replace("src/d3d9/d3d9_fixed_function.cpp",
            "D3D9FixedFunctionOptions::D3D9FixedFunctionOptions(const D3D9Options* options) {",
            "D3D9FixedFunctionOptions::D3D9FixedFunctionOptions(D3D9DeviceEx* device) {\n    const auto* options = device->GetOptions();\n    useClipDistance = device->GetDXVKDevice()->features().core.features.shaderClipDistance;")
    replace_if_present("src/d3d9/d3d9_fixed_function.cpp",
                       "      pDevice->GetOptions());", "      D3D9FixedFunctionOptions(pDevice));")
    replace("src/d3d9/d3d9_fixed_function.cpp",
            "    m_module.enableCapability(spv::CapabilityClipDistance);",
            "    if (m_options.useClipDistance)\n      m_module.enableCapability(spv::CapabilityClipDistance);")
    replace("src/d3d9/d3d9_fixed_function.cpp",
            "  void D3D9FFShaderCompiler::emitVsClipping(uint32_t vtx) {",
            "  void D3D9FFShaderCompiler::emitVsClipping(uint32_t vtx) {\n    if (!m_options.useClipDistance) return;")
    replace("src/d3d9/d3d9_adapter.cpp",
            "pCaps->MaxUserClipPlanes         = MaxClipPlanes;",
            "pCaps->MaxUserClipPlanes         = m_adapter->features().core.features.shaderClipDistance ? MaxClipPlanes : 0;")
    replace("src/d3d9/d3d9_device.cpp",
            "  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) {",
            """  HRESULT STDMETHODCALLTYPE D3D9DeviceEx::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) {
    if (State == D3DRS_CLIPPLANEENABLE && Value
     && !m_dxvkDevice->features().core.features.shaderClipDistance) {
      Logger::err("D3D9: User clip planes are unsupported by this Vulkan device");
      return D3DERR_INVALIDCALL;
    }""")
    replace_if_present("src/dxvk/dxvk_queue.cpp",
                       '#include "dxvk_queue.h"\n',
                       '''#include "dxvk_queue.h"

#if defined(__ANDROID__)
#include <cstdlib>
#include <SDL_log.h>
#endif
''')
    replace_if_present("src/dxvk/dxvk_queue.cpp",
                       "        m_device->waitForIdle();",
                       '''#if defined(__ANDROID__)
        if (status == VK_ERROR_DEVICE_LOST) {
          SDL_LogError(SDL_LOG_CATEGORY_RENDER, "DXVK v1: Vulkan device lost in %s", __func__);
          std::_Exit(2);
        }
#endif
        m_device->waitForIdle();''')
    # Upgrade checkouts that already received the old immediate-exit patch.
    replace_if_present("src/dxvk/dxvk_queue.cpp", "#include <cstdlib>\n#endif",
                       "#include <cstdlib>\n#include <SDL_log.h>\n#endif")
    replace_if_present("src/dxvk/dxvk_queue.cpp",
                       "#if defined(__ANDROID__)\n        if (status == VK_ERROR_DEVICE_LOST)\n          std::_Exit(2);\n#endif\n", "")
    replace_if_present("meson.build", "lib_sdl2    = dependency('SDL2')",
                       "lib_sdl2    = declare_dependency(include_directories: include_directories(get_option('android_sdl2_include')), link_args: ['-L' + get_option('android_sdl2_lib'), '-lSDL2'])" )
    replace_if_present("meson.build", "lib_vulkan  = dxvk_compiler.find_library('vulkan-1', dirs : dxvk_library_path)",
                       "lib_vulkan  = dxvk_compiler.find_library('vulkan')")
    replace_if_present("meson.build", "lib_vulkan  = dependency('vulkan')",
                       "lib_vulkan  = dxvk_compiler.find_library('vulkan')")
    replace_if_present("meson.build", "wrc = find_program('echo')",
                       "wrc = find_program('cmake', native: true)")
    replace_if_present("meson.build", "wrc = find_program('cmd.exe', native: true)",
                       "wrc = find_program('cmake', native: true)")
    replace_if_present("meson.build", "arguments : [ 'Ignoring: ', '@INPUT@' ]",
                       "arguments : [ '-E', 'touch', '@OUTPUT@' ]")
    replace_if_present("meson.build", "arguments : [ '/c', 'echo', 'Ignoring: ', '@INPUT@' ]",
                       "arguments : [ '-E', 'touch', '@OUTPUT@' ]")
    replace_if_present("meson.build", "glsl_compiler = find_program('glslangValidator')",
                       "glsl_compiler = find_program('glslang', native: true)")
    replace_if_present("src/d3d9/meson.build", "dependencies        : [ dxso_dep, dxvk_dep, wsi_dep ],",
                       "dependencies        : [ dxso_dep, dxvk_dep, wsi_dep, lib_sdl2 ],")
    replace_if_present("src/d3d9/meson.build", "vs_module_defs      : 'd3d9'+def_spec_ext,",
                       "vs_module_defs      : 'd3d9'+def_spec_ext,\n  link_args           : ['-L' + get_option('android_sdl2_lib'), '-lSDL2'],")
    replace_if_present("src/dxvk/meson.build", "dependencies        : [ thread_dep, vkcommon_dep ] + dxvk_extradep,",
                       "dependencies        : [ thread_dep, vkcommon_dep, lib_sdl2 ] + dxvk_extradep,")
    replace_if_present("src/dxvk/platform/dxvk_sdl2_exts.cpp",
                       "namespace dxvk {\n\n  DxvkPlatformExts DxvkPlatformExts::s_instance;",
                       "namespace dxvk {\n\n#if defined(__ANDROID__)\n  static SDL_Window* s_androidWindow = nullptr;\n\n  extern \"C\" void dxvkSetSdl2Window(SDL_Window* window) {\n    s_androidWindow = window;\n  }\n#endif\n\n  DxvkPlatformExts DxvkPlatformExts::s_instance;")
    replace_if_present("src/dxvk/platform/dxvk_sdl2_exts.cpp",
                       '''    SDL_Window* window = SDL_CreateWindow(
      "Dummy Window",
      SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED,
      1, 1,
      SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN);''',
                       '''    SDL_Window* window = nullptr;
#if defined(__ANDROID__)
    if (s_androidWindow == nullptr)
      throw DxvkError("SDL2 WSI: Application window was not supplied on Android");
    window = s_androidWindow;
#else
    window = SDL_CreateWindow(
      "Dummy Window",
      SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED,
      1, 1,
      SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN);
#endif''')
    replace_if_present("src/dxvk/platform/dxvk_sdl2_exts.cpp",
                       "    SDL_DestroyWindow(window);\n\n    return names;",
                       "#if !defined(__ANDROID__)\n    SDL_DestroyWindow(window);\n#endif\n\n    return names;")
    replace_if_present("src/d3d9/d3d9_main.cpp",
                       "#include <cstdio>\n\n#include \"../dxvk/dxvk_instance.h\"",
                       "#include <cstdio>\n\n#include \"../dxvk/dxvk_instance.h\"")
    replace_if_present("src/d3d9/d3d9_main.cpp",
                       "#include \"../dxvk/dxvk_instance.h\"",
                       "#include <cstdio>\n#include \"../dxvk/dxvk_instance.h\"")
    replace_if_present("src/d3d9/d3d9_main.cpp",
                       """    dxvk::CreateD3D9(false, &pDirect3D);

    return pDirect3D;""",
                       """    try {
      dxvk::CreateD3D9(false, &pDirect3D);
    } catch (const dxvk::DxvkError& e) {
      std::fprintf(stderr, \"DXVK v1 Direct3DCreate9: %s\\n\", e.message().c_str());
    } catch (...) {
      std::fprintf(stderr, \"DXVK v1 Direct3DCreate9: unknown native exception\\n\");
    }

    return pDirect3D;""")
    # The fork assumes desktop D3D9 features and enables several Vulkan bits
    # unconditionally. Mobile Vulkan 1.1 drivers may expose valid D3D9 paths
    # without all of them; enable only what the adapter reports.
    for feature in ("depthClamp", "depthBiasClamp", "fillModeNonSolid",
                    "sampleRateShading", "shaderClipDistance",
                    "textureCompressionBC", "multiViewport", "independentBlend"):
        replace_if_present("src/d3d9/d3d9_device.cpp",
                           "enabled.core.features.%s = VK_TRUE;" % feature,
                           "enabled.core.features.%s = supported.core.features.%s;" % (feature, feature))
    replace_if_present("src/d3d9/d3d9_device.cpp", "DxvkDeviceFeatures enabled = {};",
                       """DxvkDeviceFeatures enabled = {};
    if (!supported.core.features.shaderInt64) Logger::warn(\"Android DXVK v1 missing feature: shaderInt64\");
    if (!supported.core.features.textureCompressionBC) Logger::warn(\"Android DXVK v1 missing feature: textureCompressionBC\");
    if (!supported.core.features.multiDrawIndirect) Logger::warn(\"Android DXVK v1 missing feature: multiDrawIndirect\");
    if (!supported.core.features.multiViewport) Logger::warn(\"Android DXVK v1 missing feature: multiViewport\");
    if (!supported.core.features.shaderClipDistance) Logger::warn(\"Android DXVK v1 missing feature: shaderClipDistance\");
    if (!supported.core.features.shaderCullDistance) Logger::warn(\"Android DXVK v1 missing feature: shaderCullDistance\");""")
    replace_if_present("meson.build", "if dxvk_compiler.has_link_argument('-Wl,--file-alignment=4096'):",
                       "if target_machine.system() != 'android' and dxvk_compiler.has_link_argument('-Wl,--file-alignment=4096'):")
    write_if_missing("src/util/platform/util_env_android.cpp", r'''#include "../util_env.h"

#include <cerrno>
#include <filesystem>
#include <string>
#include <sys/prctl.h>
#include <unistd.h>

namespace dxvk::env {

std::string getExePath() {
  char path[4096] = { };
  const ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
  return len > 0 ? std::string(path, size_t(len)) : std::string();
}

void setThreadName(const std::string& name) {
  std::string truncated = name.substr(0, 15);
  prctl(PR_SET_NAME, truncated.c_str(), 0, 0, 0);
}

bool createDirectory(const std::string& path) {
  std::error_code error;
  std::filesystem::create_directories(path, error);
  return !error;
}

}
''')
    write_if_missing("src/util/platform/util_string_android.cpp", r'''#include "util_string.h"

#include <cstdint>
#include <string>

namespace dxvk::str {

std::string fromws(const WCHAR* ws) {
  if (!ws) return {};
  std::string out;
  for (const uint32_t* p = reinterpret_cast<const uint32_t*>(ws); *p; ++p) {
    uint32_t cp = *p;
    if (cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) cp = 0xfffd;
    if (cp <= 0x7f) out.push_back(char(cp));
    else if (cp <= 0x7ff) { out.push_back(char(0xc0 | (cp >> 6))); out.push_back(char(0x80 | (cp & 63))); }
    else if (cp <= 0xffff) { out.push_back(char(0xe0 | (cp >> 12))); out.push_back(char(0x80 | ((cp >> 6) & 63))); out.push_back(char(0x80 | (cp & 63))); }
    else { out.push_back(char(0xf0 | (cp >> 18))); out.push_back(char(0x80 | ((cp >> 12) & 63))); out.push_back(char(0x80 | ((cp >> 6) & 63))); out.push_back(char(0x80 | (cp & 63))); }
  }
  return out;
}

void tows(const char* mbs, WCHAR* wcs, size_t wcsLen) {
  if (!wcs || !wcsLen) return;
  size_t out = 0;
  const unsigned char* p = reinterpret_cast<const unsigned char*>(mbs ? mbs : "");
  while (*p && out + 1 < wcsLen) {
    const unsigned char lead = *p++;
    int extra = lead < 0x80 ? 0 : lead < 0xe0 ? 1 : lead < 0xf0 ? 2 : 3;
    uint32_t cp = extra == 0 ? lead : extra == 1 ? (lead & 0x1f) : extra == 2 ? (lead & 0x0f) : (lead & 7);
    if (extra && (!*p || (*p & 0xc0) != 0x80)) cp = 0xfffd, extra = 0;
    for (int i = 0; i < extra; ++i) { if ((p[i] & 0xc0) != 0x80) { extra = -1; break; } cp = (cp << 6) | (p[i] & 63); }
    if (extra < 0) { cp = 0xfffd; ++p; } else p += extra;
    wcs[out++] = static_cast<WCHAR>(cp > 0x10ffff ? 0xfffd : cp);
  }
  wcs[out] = 0;
}

}
''')
    write_if_missing("src/util/platform/util_luid_android.cpp", r'''#include "../util_luid.h"

#include "../log/log.h"

namespace dxvk {
LUID GetAdapterLUID(UINT) {
  static bool warned = false;
  if (!warned) { Logger::warn("GetAdapterLUID: Android has no native LUID; using zero"); warned = true; }
  return LUID();
}
}
''')
    replace_if_present("src/util/meson.build", "elif dxvk_platform == 'darwin'\n  util_src += util_src_darwin\nelse",
                       "elif dxvk_platform == 'darwin'\n  util_src += util_src_darwin\nelif dxvk_platform == 'android'\n  util_src += ['platform/util_env_android.cpp', 'platform/util_string_android.cpp', 'platform/util_luid_android.cpp', 'platform/thread_native.cpp']\nelse")
    # The old fork has no android_sdl2_include option; add one for the SDL
    # headers supplied by the application and use it in its native dependency.
    replace_if_present("meson_options.txt", "option('enable_tests',", "option('android_sdl2_include', type: 'string', value: '')\noption('android_sdl2_lib', type: 'string', value: '')\noption('enable_tests',")
