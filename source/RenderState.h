#pragma once

#include <d3d9.h>

#include <array>
#include <cassert>
#include <cstddef>

#ifndef FACADE_MEMBER
#define FACADE_MEMBER(type, name) \
	type& m_ ## name; \
	static inline std::size_t OFFS_ ## name = SIZE_MAX;
#endif

#ifndef INIT_FACADE_MEMBER
#define INIT_FACADE_MEMBER(obj, name) \
	m_ ## name((assert(OFFS_ ## name != SIZE_MAX), *reinterpret_cast<std::remove_reference_t<decltype(m_ ## name)>*>(reinterpret_cast<char*>(obj) + OFFS_ ## name)))
#endif

// RenderState class added stencil cache in the Polish version, EFIGS/Czech don't have it
struct RenderState;

class RenderStateFacade
{
public:
	using SamplerArray = std::array<DWORD, 8>;
	FACADE_MEMBER(DWORD, emissiveSource); // 0x88 in EFIGS, 0xBC in Polish
	FACADE_MEMBER(DWORD, blendOp); // 0x90 in EFIGS, 0xC4 in Polish
	FACADE_MEMBER(SamplerArray, maxAnisotropy); // 0xD8 in EFIGS, 0x10C in Polish
	FACADE_MEMBER(SamplerArray, borderColor); // 0x358 in EFIGS, 0x38C in Polish

public:
	RenderStateFacade(RenderState* obj)
		: INIT_FACADE_MEMBER(obj, emissiveSource),
		INIT_FACADE_MEMBER(obj, blendOp),
		INIT_FACADE_MEMBER(obj, maxAnisotropy),
		INIT_FACADE_MEMBER(obj, borderColor)
	{}
};

inline RenderState* CurrentRenderState;

inline void (*RenderState_SetSamplerState)(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value);

void RenderState_InitialiseFallbackFilters(int adapter);

DWORD RenderState_GetFallbackSamplerValue(D3DSAMPLERSTATETYPE Type, DWORD Value);
DWORD RenderState_SetTextureAnisotropicLevel(DWORD Sampler, DWORD Level);