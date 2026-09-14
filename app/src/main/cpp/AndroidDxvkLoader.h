#pragma once

#if defined(__ANDROID__)

#include <SDL_video.h>
#include <d3d9.h>

// These are the only Android-owned calls crossing into the selected DXVK DSO.
// D3D9 resource and draw calls continue through the normal COM interfaces.
void androidDxvkSetSdl2Window(SDL_Window* window);
IDirect3D9* androidDxvkCreateD3D9(UINT sdkVersion);

#endif
