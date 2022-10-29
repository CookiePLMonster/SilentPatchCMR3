#pragma once

#include <cstdint>

struct D3DViewport;

inline void (*D3DViewport_SetAspectRatio)(D3DViewport* viewport, float hfov, float vfov);
inline uint32_t (*GetResolutionWidth)();
inline uint32_t (*GetResolutionHeight)();

inline D3DViewport** gViewports;

void Graphics_Viewports_SetAspectRatios();