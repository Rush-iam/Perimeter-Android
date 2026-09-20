#ifdef __ANDROID__
#include "AndroidFrameTiming.h"
#include <SDL_error.h>
#include <SDL_filesystem.h>
#include <SDL_stdinc.h>
#include <android/log.h>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
namespace {
constexpr const char* kLogTag = "PerimeterTiming";
constexpr size_t kTimingBufferSize = 16u * 1024u * 1024u;
FILE* output = nullptr;
FILE* workOutput = nullptr;
uint64_t frameId = 0, frameStartNs = 0, renderSubmitNs = 0, presentStartNs = 0;
uint64_t d3dSubmitNs = 0;
uint64_t bumpTileCalcNs = 0;
uint64_t bumpTileTextureNs[5]{};
uint64_t bumpTileMeshNs = 0;
uint64_t bumpTileVertexWriteNs = 0;
uint64_t bumpTilePointRegionNs = 0;
uint64_t bumpTileTopologyNs = 0;
uint64_t bumpTileIndexUploadNs = 0;
uint64_t bumpTilePointInitNs = 0;
uint64_t bumpTileRegionScanNs = 0;
uint64_t bumpTileCalcCount = 0;
uint64_t tilemapLodCacheHitCount = 0;
std::string cameraMotionControlPath;
uint64_t cameraMotionLastPollNs = 0, cameraMotionStartNs = 0;
int cameraMotionLeg = 0;
bool cameraMotionActive = false;
enum class CameraMotionMode {
    None,
    HeldSquare,
    HeldSquareLong,
    ZoomOutIn,
    ZoomOutInLong,
    ZoomOutInOnce
};
CameraMotionMode cameraMotionMode = CameraMotionMode::None;
constexpr uint64_t kCameraMotionLegNs = 1000000000ull;
constexpr int kCameraMotionLegCount = 20;
constexpr int kLongCameraMotionLegCount = 60;
constexpr uint64_t kZoomMotionLegNs = 1500000000ull;
constexpr int kZoomMotionLegCount = 14;
constexpr int kLongZoomMotionLegCount = 42;
constexpr int kSingleZoomMotionLegCount = 2;
uint64_t monotonicNs() {
    timespec value{};
    clock_gettime(CLOCK_MONOTONIC, &value);
    return static_cast<uint64_t>(value.tv_sec) * 1000000000ull
        + static_cast<uint64_t>(value.tv_nsec);
}
void closeOutput() {
    if (output) {
        std::fflush(output);
        std::fclose(output);
        output = nullptr;
    }
    if (workOutput) {
        std::fflush(workOutput);
        std::fclose(workOutput);
        workOutput = nullptr;
    }
}
void flushOutput() {
    if (output) std::fflush(output);
    if (workOutput) std::fflush(workOutput);
}
void startCameraMotion(CameraMotionMode mode, uint64_t nowNs) {
    cameraMotionMode = mode;
    cameraMotionActive = true;
    cameraMotionStartNs = nowNs;
    cameraMotionLeg = 0;
    if (mode == CameraMotionMode::ZoomOutIn) {
        __android_log_print(ANDROID_LOG_INFO, kLogTag,
                            "Starting zoom camera motion: 1.5 seconds out, 1.5 seconds in, 7 cycles");
    } else if (mode == CameraMotionMode::ZoomOutInLong) {
        __android_log_print(ANDROID_LOG_INFO, kLogTag,
                            "Starting long zoom camera motion: 1.5 seconds out, 1.5 seconds in, 21 cycles");
    } else if (mode == CameraMotionMode::ZoomOutInOnce) {
        __android_log_print(ANDROID_LOG_INFO, kLogTag,
                            "Starting single zoom camera motion: 1.5 seconds out, 1.5 seconds in");
    } else {
        __android_log_print(ANDROID_LOG_INFO, kLogTag,
                            mode == CameraMotionMode::HeldSquareLong
                                ? "Starting long held-square camera motion: 15 cycles, 1 second per leg"
                                : "Starting held-square camera motion: 5 cycles, 1 second per leg");
    }
}
void updateCameraMotion() {
    if (cameraMotionControlPath.empty()) return;
    const uint64_t nowNs = monotonicNs();
    if (!cameraMotionActive) {
        if (nowNs - cameraMotionLastPollNs < 100000000ull) return;
        cameraMotionLastPollNs = nowNs;
        FILE* control = std::fopen(cameraMotionControlPath.c_str(), "r");
        if (!control) return;
        char value[64]{};
        std::fgets(value, sizeof(value), control);
        std::fclose(control);
        const std::string controlValue(value);
        if (controlValue.find("flush-frame-timing-v1") != std::string::npos) {
            std::remove(cameraMotionControlPath.c_str());
            flushOutput();
            return;
        }
        CameraMotionMode mode = CameraMotionMode::None;
        if (controlValue.find("held-square-5x-v1") != std::string::npos) {
            mode = CameraMotionMode::HeldSquare;
        } else if (controlValue.find("held-square-15x-v1") != std::string::npos) {
            mode = CameraMotionMode::HeldSquareLong;
        } else if (controlValue.find("zoom-out-in-7x-v1") != std::string::npos) {
            mode = CameraMotionMode::ZoomOutIn;
        } else if (controlValue.find("zoom-out-in-21x-v1") != std::string::npos) {
            mode = CameraMotionMode::ZoomOutInLong;
        } else if (controlValue.find("zoom-out-in-1x-v1") != std::string::npos) {
            mode = CameraMotionMode::ZoomOutInOnce;
        }
        if (mode != CameraMotionMode::None) {
            std::remove(cameraMotionControlPath.c_str());
            startCameraMotion(mode, nowNs);
        }
        return;
    }
    const uint64_t legDurationNs =
        (cameraMotionMode == CameraMotionMode::ZoomOutIn ||
         cameraMotionMode == CameraMotionMode::ZoomOutInLong ||
         cameraMotionMode == CameraMotionMode::ZoomOutInOnce)
            ? kZoomMotionLegNs : kCameraMotionLegNs;
    const int legCount = cameraMotionMode == CameraMotionMode::ZoomOutIn
        ? kZoomMotionLegCount
        : cameraMotionMode == CameraMotionMode::ZoomOutInLong
            ? kLongZoomMotionLegCount
        : cameraMotionMode == CameraMotionMode::ZoomOutInOnce
            ? kSingleZoomMotionLegCount
            : cameraMotionMode == CameraMotionMode::HeldSquareLong
                ? kLongCameraMotionLegCount : kCameraMotionLegCount;
    const int nextLeg = static_cast<int>((nowNs - cameraMotionStartNs) / legDurationNs);
    if (nextLeg <= cameraMotionLeg) return;
    if (nextLeg >= legCount) {
        cameraMotionActive = false;
        if (cameraMotionMode == CameraMotionMode::ZoomOutIn ||
            cameraMotionMode == CameraMotionMode::ZoomOutInLong ||
            cameraMotionMode == CameraMotionMode::ZoomOutInOnce) {
            __android_log_print(ANDROID_LOG_INFO, kLogTag, "Completed zoom camera motion");
        } else {
            __android_log_print(ANDROID_LOG_INFO, kLogTag, "Completed held-square camera motion");
        }
        cameraMotionMode = CameraMotionMode::None;
        return;
    }
    cameraMotionLeg = nextLeg;
}
}
void androidFrameTimingConfigure(bool enabled) {
    closeOutput();
    frameId = 0;
    if (!enabled) {
        unsetenv("DXVK_FRAME_TIMING_PATH");
        return;
    }
    char* directory = SDL_GetPrefPath("K-D Lab", "Perimeter");
    if (!directory) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                            "SDL_GetPrefPath failed: %s", SDL_GetError());
        return;
    }
    const std::string path = std::string(directory) + "frame-timing.csv";
    const std::string workPath = std::string(directory) + "frame-work.csv";
    const std::string dxvkPath = std::string(directory) + "dxvk-frame-timing.csv";
    cameraMotionControlPath = std::string(directory) + "camera-motion-control";
    SDL_free(directory);
    setenv("DXVK_FRAME_TIMING_PATH", dxvkPath.c_str(), 1);
    output = std::fopen(path.c_str(), "w");
    if (!output) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Unable to open %s", path.c_str());
        return;
    }
    // Keep diagnostic writes off the render-critical path. Files flush when
    // recording closes; the large buffers avoid periodic render-thread stalls.
    std::setvbuf(output, nullptr, _IOFBF, kTimingBufferSize);
    std::fputs("frame_id,frame_start_ns,render_submit_ns,present_start_ns,present_end_ns\n", output);
    // The harness uses this header as a readiness marker. Benchmark rows stay
    // buffered until the capture is complete.
    std::fflush(output);
    workOutput = std::fopen(workPath.c_str(), "w");
    if (workOutput) {
        std::setvbuf(workOutput, nullptr, _IOFBF, kTimingBufferSize);
        std::fputs("frame_id,event,start_ns,end_ns,value\n", workOutput);
    } else {
        __android_log_print(ANDROID_LOG_WARN, kLogTag, "Unable to open %s", workPath.c_str());
    }
    std::atexit(closeOutput);
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "Recording %s", path.c_str());
}
void androidDxvkDiagnosticConfigure(const char* maxFrameLatency, bool singlePresentInterval2) {
    (void)singlePresentInterval2;
    unsetenv("DXVK_CONFIG_FILE");
    unsetenv("DXVK_ANDROID_SWAPPY");
    const bool latencyOne = maxFrameLatency && std::string(maxFrameLatency) == "1";
    if (!latencyOne) return;
    char* directory = SDL_GetPrefPath("K-D Lab", "Perimeter");
    if (!directory) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag,
                            "SDL_GetPrefPath failed for DXVK diagnostics: %s", SDL_GetError());
        return;
    }
    const std::string path = std::string(directory) + "dxvk-diagnostic.conf";
    SDL_free(directory);
    FILE* config = std::fopen(path.c_str(), "w");
    if (!config) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Unable to open %s", path.c_str());
        return;
    }
    if (latencyOne) std::fputs("d3d9.maxFrameLatency = 1\n", config);
    std::fclose(config);
    setenv("DXVK_CONFIG_FILE", path.c_str(), 1);
    __android_log_print(ANDROID_LOG_INFO, kLogTag, "DXVK diagnostic config: %s", path.c_str());
}
void androidFrameTimingFrameStart() {
    if (!output) return;
    ++frameId;
    frameStartNs = monotonicNs();
    renderSubmitNs = presentStartNs = 0;
    d3dSubmitNs = 0;
    bumpTileCalcNs = 0;
    for (uint64_t& duration : bumpTileTextureNs) duration = 0;
    bumpTileMeshNs = 0;
    bumpTileVertexWriteNs = 0;
    bumpTilePointRegionNs = bumpTileTopologyNs = bumpTileIndexUploadNs = 0;
    bumpTilePointInitNs = bumpTileRegionScanNs = 0;
    bumpTileCalcCount = tilemapLodCacheHitCount = 0;
    updateCameraMotion();
}
void androidFrameTimingRenderSubmit() { if (output) renderSubmitNs = monotonicNs(); }
void androidFrameTimingPresentStart() { if (output) presentStartNs = monotonicNs(); }
void androidFrameTimingPresentEnd() {
    if (!output || frameStartNs == 0) return;
    const uint64_t endNs = monotonicNs();
    if (workOutput && d3dSubmitNs) {
        std::fprintf(workOutput, "%" PRIu64 ",d3d_submit_buffers_total,%" PRIu64 ",%" PRIu64 ",0\n",
                     frameId, frameStartNs, frameStartNs + d3dSubmitNs);
    }
    if (workOutput && bumpTileCalcNs) {
        std::fprintf(workOutput, "%" PRIu64 ",bump_tile_calc_total,%" PRIu64 ",%" PRIu64 ",0\n",
                     frameId, frameStartNs, frameStartNs + bumpTileCalcNs);
    }
    for (int lod = 0; lod < 5; ++lod) {
        if (workOutput && bumpTileTextureNs[lod]) {
            std::fprintf(workOutput, "%" PRIu64 ",bump_tile_texture_lod%d_total,%" PRIu64 ",%" PRIu64 ",0\n",
                         frameId, lod, frameStartNs, frameStartNs + bumpTileTextureNs[lod]);
        }
    }
    if (workOutput && bumpTileMeshNs) {
        std::fprintf(workOutput, "%" PRIu64 ",bump_tile_mesh_total,%" PRIu64 ",%" PRIu64 ",0\n",
                     frameId, frameStartNs, frameStartNs + bumpTileMeshNs);
    }
    if (workOutput && bumpTileVertexWriteNs) {
        std::fprintf(workOutput, "%" PRIu64 ",bump_tile_vertex_write_total,%" PRIu64 ",%" PRIu64 ",0\n",
                     frameId, frameStartNs, frameStartNs + bumpTileVertexWriteNs);
    }
    if (workOutput && bumpTilePointRegionNs) std::fprintf(workOutput, "%" PRIu64 ",bump_tile_point_region_total,%" PRIu64 ",%" PRIu64 ",0\n", frameId, frameStartNs, frameStartNs + bumpTilePointRegionNs);
    if (workOutput && bumpTileTopologyNs) std::fprintf(workOutput, "%" PRIu64 ",bump_tile_topology_total,%" PRIu64 ",%" PRIu64 ",0\n", frameId, frameStartNs, frameStartNs + bumpTileTopologyNs);
    if (workOutput && bumpTileIndexUploadNs) std::fprintf(workOutput, "%" PRIu64 ",bump_tile_index_upload_total,%" PRIu64 ",%" PRIu64 ",0\n", frameId, frameStartNs, frameStartNs + bumpTileIndexUploadNs);
    if (workOutput && bumpTilePointInitNs) std::fprintf(workOutput, "%" PRIu64 ",bump_tile_point_init_total,%" PRIu64 ",%" PRIu64 ",0\n", frameId, frameStartNs, frameStartNs + bumpTilePointInitNs);
    if (workOutput && bumpTileRegionScanNs) std::fprintf(workOutput, "%" PRIu64 ",bump_tile_region_scan_total,%" PRIu64 ",%" PRIu64 ",0\n", frameId, frameStartNs, frameStartNs + bumpTileRegionScanNs);
    if (workOutput) {
        if (bumpTileCalcCount) {
            std::fprintf(workOutput, "%" PRIu64 ",bump_tile_calc_count,%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
                         frameId, frameStartNs, frameStartNs, bumpTileCalcCount);
        }
        if (tilemapLodCacheHitCount) {
            std::fprintf(workOutput, "%" PRIu64 ",tilemap_lod_cache_hit_count,%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
                         frameId, frameStartNs, frameStartNs, tilemapLodCacheHitCount);
        }
    }
    std::fprintf(output, "%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
                 frameId, frameStartNs, renderSubmitNs, presentStartNs, endNs);
}
uint64_t androidFrameTimingNowNs() { return monotonicNs(); }
void androidFrameTimingRecordCompactWork(const char* event, uint64_t startNs, uint64_t endNs) {
    if (!workOutput || frameId == 0 || endNs < startNs) return;
    std::fprintf(workOutput, "%" PRIu64 ",%s,%" PRIu64 ",%" PRIu64 ",0\n",
                 frameId, event, startNs, endNs);
}
void androidFrameTimingRecordBumpTileCalc() {
    if (workOutput && frameId != 0) ++bumpTileCalcCount;
}
void androidFrameTimingRecordTilemapLodCacheHit() {
    if (workOutput && frameId != 0) ++tilemapLodCacheHitCount;
}
bool androidCameraMotionControlHeld(uint32_t control) {
    // eGameKeysControl: Up=3, Down=4, Left=5, Right=6, ZoomInc=11,
    // ZoomDec=12. This is Android-only benchmark instrumentation.
    if (!cameraMotionActive) return false;
    if (cameraMotionMode == CameraMotionMode::ZoomOutIn ||
        cameraMotionMode == CameraMotionMode::ZoomOutInLong ||
        cameraMotionMode == CameraMotionMode::ZoomOutInOnce) {
        // Zoom-dec increases camera distance (zoom out); zoom-inc decreases it.
        static constexpr uint32_t controls[] = { 12, 11 };
        return control == controls[cameraMotionLeg % 2];
    }
    static constexpr uint32_t controls[] = { 6, 3, 5, 4 };
    return control == controls[cameraMotionLeg % 4];
}
void androidFrameTimingAccumulateD3DSubmit(uint64_t startNs, uint64_t endNs) {
    if (endNs >= startNs) d3dSubmitNs += endNs - startNs;
}
void androidFrameTimingAccumulateBumpTileCalc(uint64_t startNs, uint64_t endNs) {
    if (endNs >= startNs) bumpTileCalcNs += endNs - startNs;
}
void androidFrameTimingAccumulateBumpTileTexture(int lod, uint64_t startNs, uint64_t endNs) {
    if (lod >= 0 && lod < 5 && endNs >= startNs) bumpTileTextureNs[lod] += endNs - startNs;
}
void androidFrameTimingAccumulateBumpTileMesh(uint64_t startNs, uint64_t endNs) {
    if (endNs >= startNs) bumpTileMeshNs += endNs - startNs;
}
void androidFrameTimingAccumulateBumpTileVertexWrite(uint64_t startNs, uint64_t endNs) {
    if (endNs >= startNs) bumpTileVertexWriteNs += endNs - startNs;
}
void androidFrameTimingAccumulateBumpTilePointRegion(uint64_t startNs, uint64_t endNs) { if (endNs >= startNs) bumpTilePointRegionNs += endNs - startNs; }
void androidFrameTimingAccumulateBumpTileTopology(uint64_t startNs, uint64_t endNs) { if (endNs >= startNs) bumpTileTopologyNs += endNs - startNs; }
void androidFrameTimingAccumulateBumpTileIndexUpload(uint64_t startNs, uint64_t endNs) { if (endNs >= startNs) bumpTileIndexUploadNs += endNs - startNs; }
void androidFrameTimingAccumulateBumpTilePointInit(uint64_t startNs, uint64_t endNs) { if (endNs >= startNs) bumpTilePointInitNs += endNs - startNs; }
void androidFrameTimingAccumulateBumpTileRegionScan(uint64_t startNs, uint64_t endNs) { if (endNs >= startNs) bumpTileRegionScanNs += endNs - startNs; }
#endif
