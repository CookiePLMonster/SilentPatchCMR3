#include "RenderState.h"

#include "Graphics.h"

static D3DTEXTUREFILTERTYPE MinFilterFallBack[4];
static D3DTEXTUREFILTERTYPE MagFilterFallBack[4];
static D3DTEXTUREFILTERTYPE MipFilterFallBack[4];

static D3DTEXTUREFILTERTYPE RenderState_SetTextureMinFilterFallBack(D3DTEXTUREFILTERTYPE filter, D3DTEXTUREFILTERTYPE fallback)
{
	return std::exchange(MinFilterFallBack[filter], fallback);
}

static D3DTEXTUREFILTERTYPE RenderState_SetTextureMagFilterFallBack(D3DTEXTUREFILTERTYPE filter, D3DTEXTUREFILTERTYPE fallback)
{
	return std::exchange(MagFilterFallBack[filter], fallback);
}

static D3DTEXTUREFILTERTYPE RenderState_SetTextureMipFilterFallBack(D3DTEXTUREFILTERTYPE filter, D3DTEXTUREFILTERTYPE fallback)
{
	return std::exchange(MipFilterFallBack[filter], fallback);
}


void RenderState_InitialiseFallbackFilters(int adapter)
{
	D3DCAPS9 caps;
	Graphics_GetAdapterCaps(&caps, adapter);

	const bool hasAnisotropy = CMR_GetAnisotropicLevel() > 1;

	// Min filter
	{
		D3DTEXTUREFILTERTYPE filterForNone, filterForPoint, filterForLinear, filterForAnisotropic;

		filterForNone = D3DTEXF_NONE;
		if ((caps.TextureFilterCaps & D3DPTFILTERCAPS_MINFPOINT) != 0)
		{
			filterForPoint = D3DTEXF_POINT;
		}
		else
		{
			filterForPoint = filterForNone;
		}
		if ((caps.TextureFilterCaps & D3DPTFILTERCAPS_MINFLINEAR) != 0)
		{
			filterForLinear = D3DTEXF_LINEAR;
		}
		else
		{
			filterForLinear = filterForPoint;
		}
		if (hasAnisotropy && (caps.TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC) != 0)
		{
			filterForAnisotropic = D3DTEXF_ANISOTROPIC;
		}
		else
		{
			filterForAnisotropic = filterForLinear;
		}

		RenderState_SetTextureMinFilterFallBack(D3DTEXF_NONE, filterForNone);
		RenderState_SetTextureMinFilterFallBack(D3DTEXF_POINT, filterForPoint);
		RenderState_SetTextureMinFilterFallBack(D3DTEXF_LINEAR, filterForLinear);
		RenderState_SetTextureMinFilterFallBack(D3DTEXF_ANISOTROPIC, filterForAnisotropic);
	}

	// Mag filter
	{
		D3DTEXTUREFILTERTYPE filterForNone, filterForPoint, filterForLinear, filterForAnisotropic;

		filterForNone = D3DTEXF_NONE;
		if ((caps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFPOINT) != 0)
		{
			filterForPoint = D3DTEXF_POINT;
		}
		else
		{
			filterForPoint = filterForNone;
		}
		if ((caps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFLINEAR) != 0)
		{
			filterForLinear = D3DTEXF_LINEAR;
		}
		else
		{
			filterForLinear = filterForPoint;
		}
		if (hasAnisotropy && (caps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFANISOTROPIC) != 0)
		{
			filterForAnisotropic = D3DTEXF_ANISOTROPIC;
		}
		else
		{
			filterForAnisotropic = filterForLinear;
		}

		RenderState_SetTextureMagFilterFallBack(D3DTEXF_NONE, filterForNone);
		RenderState_SetTextureMagFilterFallBack(D3DTEXF_POINT, filterForPoint);
		RenderState_SetTextureMagFilterFallBack(D3DTEXF_LINEAR, filterForLinear);
		RenderState_SetTextureMagFilterFallBack(D3DTEXF_ANISOTROPIC, filterForAnisotropic);
	}

	// Mip filter
	{
		D3DTEXTUREFILTERTYPE filterForNone, filterForPoint, filterForLinear, filterForAnisotropic;

		filterForNone = D3DTEXF_NONE;
		if ((caps.TextureFilterCaps & D3DPTFILTERCAPS_MIPFPOINT) != 0)
		{
			filterForPoint = D3DTEXF_POINT;
		}
		else
		{
			filterForPoint = filterForNone;
		}
		if ((caps.TextureFilterCaps & D3DPTFILTERCAPS_MIPFLINEAR) != 0)
		{
			filterForLinear = D3DTEXF_LINEAR;
		}
		else
		{
			filterForLinear = filterForPoint;
		}
		filterForAnisotropic = filterForLinear;

		RenderState_SetTextureMipFilterFallBack(D3DTEXF_NONE, filterForNone);
		RenderState_SetTextureMipFilterFallBack(D3DTEXF_POINT, filterForPoint);
		RenderState_SetTextureMipFilterFallBack(D3DTEXF_LINEAR, filterForLinear);
		RenderState_SetTextureMipFilterFallBack(D3DTEXF_ANISOTROPIC, filterForAnisotropic);
	}
}

DWORD RenderState_GetFallbackSamplerValue(D3DSAMPLERSTATETYPE Type, DWORD Value)
{
#ifndef NDEBUG
	if (Type >= D3DSAMP_MAGFILTER && Type <= D3DSAMP_MIPFILTER && Value == D3DTEXF_ANISOTROPIC)
	{
		if (GetAsyncKeyState(VK_F2) & 0x8000)
		{
			return D3DTEXF_LINEAR;
		}
	}
#endif
	switch (Type)
	{
	case D3DSAMP_MAGFILTER:
		return MagFilterFallBack[Value];
	case D3DSAMP_MINFILTER:
		return MinFilterFallBack[Value];
	case D3DSAMP_MIPFILTER:
		return MipFilterFallBack[Value];
	default:
		break;
	}

	return Value;
}

DWORD RenderState_SetTextureAnisotropicLevel(DWORD Sampler, DWORD Level)
{
	RenderStateFacade CurrentRenderStateF(CurrentRenderState);
	const DWORD currentLevel = CurrentRenderStateF.m_maxAnisotropy[Sampler];
	if (currentLevel != Level)
	{
		RenderState_SetSamplerState(Sampler, D3DSAMP_MAXANISOTROPY, Level);
		CurrentRenderStateF.m_maxAnisotropy[Sampler] = Level;
	}
	return currentLevel;
}
