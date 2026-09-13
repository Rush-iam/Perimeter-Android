#pragma once
#include <cstdint>

#ifdef __ANDROID__
void androidFrameTimingConfigure(bool enabled);
void androidDxvkDiagnosticConfigure(const char* maxFrameLatency, bool singlePresentInterval2);
void androidFrameTimingFrameStart();
void androidFrameTimingRenderSubmit();
void androidFrameTimingPresentStart();
void androidFrameTimingPresentEnd();
uint64_t androidFrameTimingNowNs();
void androidFrameTimingRecordCompactWork(const char* event, uint64_t startNs, uint64_t endNs);
void androidFrameTimingRecordBumpTileCalc();
void androidFrameTimingRecordTilemapLodCacheHit();
bool androidCameraMotionControlHeld(uint32_t control);
void androidFrameTimingAccumulateD3DSubmit(uint64_t startNs, uint64_t endNs);
void androidFrameTimingAccumulateBumpTileCalc(uint64_t startNs, uint64_t endNs);
void androidFrameTimingAccumulateBumpTileTexture(int lod, uint64_t startNs, uint64_t endNs);
void androidFrameTimingAccumulateBumpTileMesh(uint64_t startNs, uint64_t endNs);
void androidFrameTimingAccumulateBumpTileVertexWrite(uint64_t startNs, uint64_t endNs);
void androidFrameTimingAccumulateBumpTilePointRegion(uint64_t startNs, uint64_t endNs);
void androidFrameTimingAccumulateBumpTileTopology(uint64_t startNs, uint64_t endNs);
void androidFrameTimingAccumulateBumpTileIndexUpload(uint64_t startNs, uint64_t endNs);
void androidFrameTimingAccumulateBumpTilePointInit(uint64_t startNs, uint64_t endNs);
void androidFrameTimingAccumulateBumpTileRegionScan(uint64_t startNs, uint64_t endNs);
#endif
