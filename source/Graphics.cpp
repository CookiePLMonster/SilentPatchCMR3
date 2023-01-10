#include "Graphics.h"

#include "Utils/MemoryMgr.h"
#include <cmath>
#include <Shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")

using namespace Graphics::Patches;

static void RecalculateMoviesDimensions();

static uint32_t gAnisotropicLevel = 1;

void CMR_FE_SetAnisotropicLevel(uint32_t level)
{
	gAnisotropicLevel = 1 << level;
}

uint32_t CMR_FE_GetAnisotropicLevel()
{
	DWORD Index = 0;
	return _BitScanForward(&Index, gAnisotropicLevel) != 0 ? Index : 0;
}

uint32_t CMR_FE_GetMaxAnisotropicLevel(int adapter)
{
	D3DCAPS9 caps;
	Graphics_GetAdapterCaps(&caps, adapter);
	return (caps.RasterCaps & D3DPRASTERCAPS_ANISOTROPY) != 0 ? caps.MaxAnisotropy : 1;
}

uint32_t CMR_GetAnisotropicLevel()
{
	const Graphics_Config& config = Graphics_GetCurrentConfig();
	return std::min(gAnisotropicLevel, CMR_FE_GetMaxAnisotropicLevel(config.m_adapter));
}

const Graphics_Config& Graphics_GetCurrentConfig()
{
	return *gGraphicsConfig;
}

float GetScaledResolutionWidth()
{
	return UIFullyPatched ? 480.0f * (static_cast<float>(Graphics_GetScreenWidth()) / Graphics_GetScreenHeight()) : 640.0f;
}

void Core_Texture_SetFilteringMethod(D3DTexture* texture, uint32_t min, uint32_t mag, uint32_t mip)
{
	if (texture != nullptr)
	{
		texture->m_minFilter = min;
		texture->m_magFilter = mag;
		texture->m_mipFilter = mip;
	}
}

void Texture_DestroyManaged(D3DTexture** texture)
{
	if (*texture != nullptr)
	{
		Core_Texture_Destroy(*texture);
		*texture = nullptr;
	}
}

OSD_Element* OSD_Element_Init_Center(OSD_Element* element, int posX, int posY, int width, int height, int a6, int a7, int a8, int a9, int a10, int a11)
{
	const float scaledWidth = GetScaledResolutionWidth();
	const int offset = posX - 320;
	return OSD_Element_Init(element, static_cast<int>(scaledWidth / 2 + offset), posY, width, height, a6, a7, a8, a9, a10, a11);
}

OSD_Element* OSD_Element_Init_RightAlign(OSD_Element* element, int posX, int posY, int width, int height, int a6, int a7, int a8, int a9, int a10, int a11)
{
	const float scaledWidth = GetScaledResolutionWidth();
	const int offset = 640 - posX;
	return OSD_Element_Init(element, static_cast<int>(scaledWidth - offset), posY, width, height, a6, a7, a8, a9, a10, a11);
}

OSD_Element* OSD_Element_Init(OSD_Element* element, int posX, int posY, int width, int height, int a6, int a7, int a8, int a9, int a10, int a11)
{
	element->field_0 = 0;
	element->field_40 = 0;
	element->field_44 = 0;
	element->field_48 = 0;
	element->m_posY = posY;
	element->m_posX = posX;
	element->m_height = height;
	element->m_width = width;
	element->field_18 = a7;
	element->field_54 = a6;
	element->field_28 = a9;
	element->field_1C = a8;
	element->field_5C = a11;
	element->field_2C = a10;
	return element;
}

void Core_Blitter2D_Rect2D_G_Center(BlitRect2D_G* rects, uint32_t numRectangles)
{
	const float resolutionWidth = static_cast<float>(Graphics_GetScreenWidth());
	const float scaledOldCenter = 320.0f * (resolutionWidth / GetScaledResolutionWidth());
	const float scaledRealCenter = resolutionWidth / 2.0f;
	const float offset = scaledRealCenter - scaledOldCenter;
	for (uint32_t i = 0; i < numRectangles; ++i)
	{
		// Those rectangles are already multiplied for resolution
		rects[i].X[0] += offset;
		rects[i].X[1] += offset;
	}
	Core_Blitter2D_Rect2D_G(rects, numRectangles);
}

void Core_Blitter2D_Line2D_G_Center(BlitLine2D_G* lines, uint32_t numLines)
{
	const float resolutionWidth = static_cast<float>(Graphics_GetScreenWidth());
	const float scaledOldCenter = 320.0f * (resolutionWidth / GetScaledResolutionWidth());
	const float scaledRealCenter = resolutionWidth / 2.0f;
	const float offset = scaledRealCenter - scaledOldCenter;
	for (uint32_t i = 0; i < numLines; ++i)
	{
		// Those rectangles are already multiplied for resolution
		lines[i].X[0] += offset;
		lines[i].X[1] += offset;
	}
	Core_Blitter2D_Line2D_G(lines, numLines);
}

void Core_Blitter2D_Rect2D_GT_CenterHalf(BlitRect2D_GT* rects, uint32_t numRectangles)
{
	const float resolutionWidth = static_cast<float>(Graphics_GetScreenWidth());
	const float scaledOldCenter = 160.0f * (resolutionWidth / GetScaledResolutionWidth());
	const float scaledRealCenter = resolutionWidth / 4.0f;
	const float offset = scaledRealCenter - scaledOldCenter;
	for (uint32_t i = 0; i < numRectangles; ++i)
	{
		// Those rectangles are already multiplied for resolution
		rects[i].X[0] += offset;
		rects[i].X[1] += offset;
	}
	Core_Blitter2D_Rect2D_GT(rects, numRectangles);
}

void Core_Blitter2D_Rect2D_GT_RightAlign(BlitRect2D_GT* rects, uint32_t numRectangles)
{
	const float resolutionWidth = static_cast<float>(Graphics_GetScreenWidth());
	const float scaledOldRight = 640.0f * (resolutionWidth / GetScaledResolutionWidth());
	const float offset = resolutionWidth - scaledOldRight;
	for (uint32_t i = 0; i < numRectangles; ++i)
	{
		// Those rectangles are already multiplied for resolution
		rects[i].X[0] += offset;
		rects[i].X[1] += offset;
	}
	Core_Blitter2D_Rect2D_GT(rects, numRectangles);
}

void HandyFunction_Draw2DBox_Stretch(int posX, int posY, int width, int height, int color)
{
	const float widthScale = GetScaledResolutionWidth() / 640.0f;
	HandyFunction_Draw2DBox(posX, posY, static_cast<int>(std::ceil(width * widthScale)), height, color);
}

void HandyFunction_Draw2DBox_Center(int posX, int posY, int width, int height, int color)
{
	const float scaledWidth = GetScaledResolutionWidth();
	const int offset = posX - 320;
	HandyFunction_Draw2DBox(static_cast<int>(scaledWidth / 2 + offset), posY, width + 1, height, color);
}

void HandyFunction_Draw2DBox_RightAlign(int posX, int posY, int width, int height, int color)
{
	const float scaledWidth = GetScaledResolutionWidth();
	const int offset = 640 - posX;
	HandyFunction_Draw2DBox(static_cast<int>(scaledWidth - offset), posY, width + 1, height, color);
}

void HandyFunction_Draw2DLineFromTo_Center(float x1, float y1, float x2, float y2, uint32_t* z, uint32_t color)
{
	const float scaledWidth = GetScaledResolutionWidth();
	HandyFunction_Draw2DLineFromTo((scaledWidth / 2.0f) + x1 - 320.0f, y1, (scaledWidth / 2.0f) + x2 - 320.0f, y2, z, color);
}

void CMR3Font_BlitText_Center(uint8_t a1, const char* text, int16_t posX, int16_t posY, uint32_t color, int align)
{
	const float scaledWidth = GetScaledResolutionWidth();
	const int offset = posX - 320;
	CMR3Font_BlitText(a1, text, static_cast<int16_t>(scaledWidth / 2 + offset), posY, color, align);
}

void CMR3Font_BlitText_RightAlign(uint8_t a1, const char* text, int16_t posX, int16_t posY, uint32_t color, int align)
{
	const float scaledWidth = GetScaledResolutionWidth();
	const int offset = 640 - posX;
	CMR3Font_BlitText(a1, text, static_cast<int16_t>(scaledWidth - offset), posY, color, align);
}

void Keyboard_DrawTextEntryBox_Center(int posX, int posY, int a3, int a4, uint32_t a5, int a6)
{
	const float scaledWidth = GetScaledResolutionWidth();
	const int offset = posX - 320;
	Keyboard_DrawTextEntryBox(static_cast<int>(scaledWidth / 2 + offset), posY, a3, a4, a5, a6);
}

uint32_t HandyFunction_AlphaCombineFlat(uint32_t color, uint32_t alpha)
{
	const uint8_t colorAlpha = (color >> 24) & 0xFF;
	const uint32_t colorBlendedAlpha = (colorAlpha * alpha) / 255;
	return (color & 0xFFFFFF) | (colorBlendedAlpha << 24);
}

void Graphics_Viewports_SetAspectRatios()
{
	const float Width = static_cast<float>(Graphics_GetScreenWidth());
	const float Height = static_cast<float>(Graphics_GetScreenHeight());
	const float InvAR = Height / Width;
	const float TargetAR = 4.0f / 3.0f;

	// Main viewport
	Viewport_SetAspectRatio(gViewports[0], TargetAR * InvAR, TargetAR);

	// Horizontal splitscreen - top
	Viewport_SetAspectRatio(gViewports[1], TargetAR * InvAR, TargetAR * 2.0f);

	// Horizontal splitscreen - bottom
	{
		const float HeightSplit = Height + 50.0f;
		const float InvARSplit = HeightSplit / Width;
		Viewport_SetAspectRatio(gViewports[2], TargetAR * InvARSplit, TargetAR * 2.0f);
	}

	// Vertical splitscreen - left
	{
		const float WidthSplit = Width - 50.0f;
		const float InvARSplit = Height / WidthSplit;
		Viewport_SetAspectRatio(gViewports[3], TargetAR * InvARSplit * 2.0f, TargetAR);
	}

	// Vertical splitscreen - right
	Viewport_SetAspectRatio(gViewports[4], TargetAR * InvAR * 2.0f, TargetAR);

	// 4p splitscreen - top left, top right
	Viewport_SetAspectRatio(gViewports[5], TargetAR * InvAR, TargetAR);
	Viewport_SetAspectRatio(gViewports[6], TargetAR * InvAR, TargetAR);

	// 4p splitscreen - bottom left, bottom right
	{
		const float HeightSplit = Height + 50.0f;
		const float InvARSplit = HeightSplit / Width;
		Viewport_SetAspectRatio(gViewports[7], TargetAR * InvARSplit, TargetAR);
		Viewport_SetAspectRatio(gViewports[8], TargetAR * InvARSplit, TargetAR);
	}
}

static OSD_Data OSDPositions_Original[4];
static OSD_Data2 OSDPositionsMulti_Original[4];
static Object_StartLight Object_StartLightOriginal[2];
void OSD_Main_SetUpStructsForWidescreen()
{
	// Fix broken UI elements first

	// Split times width for vertical splitscreen
	orgOSDPositions[2].m_nCertinaBoxWidth = orgOSDPositions[0].m_nCertinaBoxWidth;
	orgOSDPositions[3].m_nCertinaBoxWidth = orgOSDPositions[1].m_nCertinaBoxWidth;

	// Misplaced LEDs

	// 1p, analog tachometer, both LEDs misplaced
	for (size_t i = 0; i < 2; i++)
	{
		orgOSDPositions[i].m_nLEDOffsetX -= 1;

		orgOSDPositions[i].m_nLEDOffsetY += 1;
		orgOSDPositions[i].m_nGearOffsetY += 1;
	}

	memcpy(OSDPositions_Original, orgOSDPositions, sizeof(OSDPositions_Original));
	memcpy(OSDPositionsMulti_Original, orgOSDPositionsMulti, sizeof(OSDPositionsMulti_Original));
	memcpy(Object_StartLightOriginal, orgStartLightData, sizeof(Object_StartLightOriginal));
}

static float gAspectRatioMult;
void RecalculateUI()
{
	const int32_t ResWidth = Graphics_GetScreenWidth();
	const int32_t ResHeight = Graphics_GetScreenHeight();

	gAspectRatioMult = (4.0f * ResHeight) / (3.0f * ResWidth);

	const float ScaledResWidth = 640.0f / gAspectRatioMult;

	auto centered = [ScaledResWidth](const auto& val)
	{
		const auto offset = val - 320;
		return static_cast<decltype(val)>((ScaledResWidth / 2) + offset);
	};

	auto centeredHalf = [ScaledResWidth](const auto& val)
	{
		const auto offset = val - 160;
		return static_cast<decltype(val)>((ScaledResWidth / 4) + offset);
	};

	auto right = [ScaledResWidth](const auto& val)
	{
		const auto offset = 640 - val;
		return static_cast<decltype(val)>(ScaledResWidth - offset);
	};

	// Adjust for 590 * X / 640
	auto centered_adj = [ScaledResWidth, &centered](const auto& val)
	{
		return 640 * centered((static_cast<int>(ScaledResWidth) - 50) * val / static_cast<int>(ScaledResWidth)) / 590;
	};

	{
		const HINSTANCE mainModuleInstance = GetModuleHandle(nullptr);
		auto Protect = ScopedUnprotect::UnprotectSectionOrFullModule(mainModuleInstance, ".text");
		ScopedUnprotect::Section Protect2(mainModuleInstance, ".rdata");
	
		*UI_resolutionWidthMult[0] = *UI_resolutionWidthMult[1] = gAspectRatioMult / 640.0f;
		*UI_CoutdownPosXVertical[0] = *UI_CoutdownPosXVertical[1] = centeredHalf(146);

		*UI_MenuBarTextDrawLimit = static_cast<int32_t>(ScaledResWidth * 1.4375f + 1.0f); // Original magic constant, 921 for 640px

		for (const auto& item : UI_CenteredElements)
		{
			std::visit([&](const auto& val)
			{
				*val.first = centered(val.second);
			}, item);
		}

		// Menus, right column, default - 393 (640 - 247)
		for (const auto& item : UI_RightAlignElements)
		{
			std::visit([&](const auto& val)
			{
				*val.first = right(val.second);
			}, item);
		}

		RecalculateMoviesDimensions();
	}

	// Update OSD data
	{
		for (size_t i = 0; i < 4; i++)
		{
			orgOSDPositions[i].m_CoDriverX = centered(OSDPositions_Original[i].m_CoDriverX);
		}

		// [0] is aligned to right, the rest is centered
		orgOSDPositionsMulti[0].m_CoDriverX = right(OSDPositionsMulti_Original[0].m_CoDriverX);
		orgOSDPositionsMulti[0].m_TachoX = right(OSDPositionsMulti_Original[0].m_TachoX);
		for (size_t i = 1; i < 4; i++)
		{
			orgOSDPositionsMulti[i].m_CoDriverX = centeredHalf(OSDPositionsMulti_Original[i].m_CoDriverX);

			orgOSDPositionsMulti[i].m_StageProgressX = centered(OSDPositionsMulti_Original[i].m_StageProgressX) + 1; // Should this be half the line width maybe?
			orgOSDPositionsMulti[i].m_SplitTimesX = centered_adj(OSDPositionsMulti_Original[i].m_SplitTimesX);
		}
	}

	// Update start lights data
	for (size_t i = 0; i < 2; i++)
	{
		orgStartLightData[i].m_posX = centered(Object_StartLightOriginal[i].m_posX);
	}

	// Force the tacho to reinitialise
	*UI_TachoInitialised = 0;
}

void Viewport_SetDimensions(D3DViewport* viewport, int left, int top, int right, int bottom)
{
	if (viewport == nullptr)
	{
		viewport = *gpFullScreenViewport;
	}

	const int32_t ResWidth = Graphics_GetScreenWidth();
	const int32_t ResHeight = Graphics_GetScreenHeight();

	viewport->m_left = left;
	viewport->m_top = top;
	viewport->m_width = right;
	viewport->m_height = bottom;
	viewport->m_horFov = (4.0f * bottom) / (3.0f * right);
	viewport->m_leftScale = static_cast<float>(left) / ResWidth;
	viewport->m_topScale = static_cast<float>(top) / ResHeight;
	viewport->m_rightScale = static_cast<float>(right) / ResWidth;
	viewport->m_bottomScale = static_cast<float>(bottom) / ResHeight;
	viewport->m_vertFov = 4.0f/3.0f;
}

D3DViewport* Viewport_GetCurrent()
{
	return *gpCurrentViewport;
}

static bool bCurrentMoviePillarboxed = false;
static void RecalculateMoviesDimensions()
{
	const float ScaledWidth = GetScaledResolutionWidth();
	if (bCurrentMoviePillarboxed || gAspectRatioMult >= 1.0f)
	{
		*UI_MovieX1 = ScaledWidth / 2.0f - 320.0f - 0.5f;
		*UI_MovieY1 = -0.5f;
		*UI_MovieX2 = ScaledWidth / 2.0f + 320.0f + 0.5f;
		*UI_MovieY2 = 480.5f;
	}
	else
	{
		// Wider than 4:3, cut off top/bottom
		const float DesiredHeight = ScaledWidth * 3.0f / 4.0f;
		*UI_MovieX1 = -0.5f;
		*UI_MovieY1 = (240.0f - (DesiredHeight / 2.0f)) - 0.5f;
		*UI_MovieX2 = ScaledWidth + 0.5f;
		*UI_MovieY2 = (240.0f + (DesiredHeight / 2.0f)) + 0.5f;
	}
}

void (*orgSetMovieDirectory)(const char* path);
void SetMovieDirectory_SetDimensions(const char* path)
{
	// intros are pillarboxed, the rest is filled
	// Movies fill the screen, cutting off the edges
	bCurrentMoviePillarboxed = StrStrIA(path, "intros") != nullptr;
	{
		auto Protect = ScopedUnprotect::UnprotectSectionOrFullModule( GetModuleHandle( nullptr ), ".text" );
		RecalculateMoviesDimensions();
	}
	orgSetMovieDirectory(path);
}
