#include "Graphics.h"

#include "Utils/MemoryMgr.h"
#include <cmath>
#include <Shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")

using namespace Graphics::Patches;

static void RecalculateMoviesDimensions();

float GetScaledResolutionWidth()
{
	return 480.0f * (static_cast<float>(GetResolutionWidth()) / GetResolutionHeight());
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

void Core_Blitter2D_Rect2D_G_Center(float* data, uint32_t numRectangles)
{
	const float resolutionWidth = static_cast<float>(GetResolutionWidth());
	const float scaledOldCenter = 320.0f * (resolutionWidth / GetScaledResolutionWidth());
	const float scaledRealCenter = resolutionWidth / 2.0f;
	const float offset = scaledRealCenter - scaledOldCenter;
	for (uint32_t i = 0; i < numRectangles; ++i)
	{
		// Those rectangles are already multiplied for resolution
		float* rect = &data[9 * i];
		rect[4] += offset;
		rect[5] += offset;
	}
	Core_Blitter2D_Rect2D_G(data, numRectangles);
}

void Core_Blitter2D_Line2D_G_Center(float* data, uint32_t numLines)
{
	const float resolutionWidth = static_cast<float>(GetResolutionWidth());
	const float scaledOldCenter = 320.0f * (resolutionWidth / GetScaledResolutionWidth());
	const float scaledRealCenter = resolutionWidth / 2.0f;
	const float offset = scaledRealCenter - scaledOldCenter;
	for (uint32_t i = 0; i < numLines; ++i)
	{
		// Those rectangles are already multiplied for resolution
		float* rect = &data[8 * i];
		rect[2] += offset;
		rect[3] += offset;
	}
	Core_Blitter2D_Line2D_G(data, numLines);
}

void Core_Blitter2D_Rect2D_GT_RightAlign(float* data, uint32_t numRectangles)
{
	const float resolutionWidth = static_cast<float>(GetResolutionWidth());
	const float scaledOldRight = 640.0f * (resolutionWidth / GetScaledResolutionWidth());
	const float offset = resolutionWidth - scaledOldRight;
	for (uint32_t i = 0; i < numRectangles; ++i)
	{
		// Those rectangles are already multiplied for resolution
		float* rect = &data[13 * i];
		rect[8] += offset;
		rect[9] += offset;
	}
	Core_Blitter2D_Rect2D_GT(data, numRectangles);
}

void HandyFunction_Draw2DBox_Stretch(int posX, int posY, int width, int height, int color)
{
	const float widthScale = GetScaledResolutionWidth() / 640.0f;
	HandyFunction_Draw2DBox(posX, posY, static_cast<int>(std::ceil(width * widthScale)), height, color);
}

void HandyFunction_Draw2DBox_RightAlign(int posX, int posY, int width, int height, int color)
{
	const float scaledWidth = GetScaledResolutionWidth();
	const int offset = 640 - posX;
	HandyFunction_Draw2DBox(static_cast<int>(scaledWidth - offset), posY, width + 1, height, color);
}

void CMR3Font_BlitText_Center(uint8_t a1, const char* text, int posX, int posY, int a5, char a6)
{
	const float scaledWidth = GetScaledResolutionWidth();
	const int offset = posX - 320;
	CMR3Font_BlitText(a1, text, static_cast<int>(scaledWidth / 2 + offset), posY, a5, a6);
}

void CMR3Font_BlitText_RightAlign(uint8_t a1, const char* text, int posX, int posY, int a5, char a6)
{
	const float scaledWidth = GetScaledResolutionWidth();
	const int offset = 640 - posX;
	CMR3Font_BlitText(a1, text, static_cast<int>(scaledWidth - offset), posY, a5, a6);
}

void Keyboard_DrawTextEntryBox_Center(int posX, int posY, int a3, int a4, uint32_t a5, int a6)
{
	const float scaledWidth = GetScaledResolutionWidth();
	const int offset = posX - 320;
	Keyboard_DrawTextEntryBox(static_cast<int>(scaledWidth / 2 + offset), posY, a3, a4, a5, a6);
}

void Graphics_Viewports_SetAspectRatios()
{
	const float Width = static_cast<float>(GetResolutionWidth());
	const float Height = static_cast<float>(GetResolutionHeight());
	const float InvAR = Height / Width;
	const float TargetAR = 4.0f / 3.0f;

	// TODO: Adjust FOV here
	auto setAspectRatio = [](D3DViewport* viewport, float hfov, float vfov)
	{
		const float FOV = 1.0f;
		D3DViewport_SetAspectRatio(viewport, hfov * FOV, vfov * FOV);
	};

	// Main viewport
	setAspectRatio(gViewports[0], TargetAR * InvAR, TargetAR);

	// Horizontal splitscreen - top
	setAspectRatio(gViewports[1], TargetAR * InvAR, TargetAR * 2.0f);

	// Horizontal splitscreen - bottom
	{
		const float HeightSplit = Height + 50.0f;
		const float InvARSplit = HeightSplit / Width;
		setAspectRatio(gViewports[2], TargetAR * InvARSplit, TargetAR * 2.0f);
	}

	// Vertical splitscreen - left
	{
		const float WidthSplit = Width - 50.0f;
		const float InvARSplit = Height / WidthSplit;
		setAspectRatio(gViewports[3], TargetAR * InvARSplit * 2.0f, TargetAR);
	}

	// Vertical splitscreen - right
	setAspectRatio(gViewports[4], TargetAR * InvAR * 2.0f, TargetAR);

	// 4p splitscreen - top left, top right
	setAspectRatio(gViewports[5], TargetAR * InvAR, TargetAR);
	setAspectRatio(gViewports[6], TargetAR * InvAR, TargetAR);

	// 4p splitscreen - bottom left, bottom right
	{
		const float HeightSplit = Height + 50.0f;
		const float InvARSplit = HeightSplit / Width;
		setAspectRatio(gViewports[7], TargetAR * InvARSplit, TargetAR);
		setAspectRatio(gViewports[8], TargetAR * InvARSplit, TargetAR);
	}
}

static OSD_Data OSD_DataOriginal[4];
static OSD_Data2 OSD_Data2Original[4];
static Object_StartLight Object_StartLightOriginal[2];
void OSD_Main_SetUpStructsForWidescreen()
{
	memcpy(OSD_DataOriginal, orgOSDData, sizeof(OSD_DataOriginal));
	memcpy(OSD_Data2Original, orgOSDData2, sizeof(OSD_Data2Original));
	memcpy(Object_StartLightOriginal, orgStartLightData, sizeof(Object_StartLightOriginal));
}

static float gAspectRatioMult;
static void RecalculateUI()
{
	const int32_t ResWidth = GetResolutionWidth();
	const int32_t ResHeight = GetResolutionHeight();

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

	{
		auto Protect = ScopedUnprotect::UnprotectSectionOrFullModule( GetModuleHandle( nullptr ), ".text" );
		ScopedUnprotect::Section Protect2( GetModuleHandle( nullptr ), ".rdata" );
	
		*UI_resolutionWidthMult = gAspectRatioMult / 640.0f;
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
			orgOSDData[i].m_CoDriverX = centered(OSD_DataOriginal[i].m_CoDriverX);
		}

		// [0] is aligned to right, the rest is centered
		orgOSDData2[0].m_CoDriverX = right(OSD_Data2Original[0].m_CoDriverX);
		for (size_t i = 1; i < 4; i++)
		{
			orgOSDData2[i].m_CoDriverX = centeredHalf(OSD_Data2Original[i].m_CoDriverX);
		}
	}

	// Update start lights data
	for (size_t i = 0; i < 2; i++)
	{
		orgStartLightData[i].m_posX = centered(Object_StartLightOriginal[i].m_posX);
	}
}

void (*orgD3D_Initialise)(void* param);
void D3D_Initialise_RecalculateUI(void* param)
{
	orgD3D_Initialise(param);
	RecalculateUI();
}

void (*orgD3D_AfterReinitialise)(void* param);
void D3D_AfterReinitialise_RecalculateUI(void* param)
{
	orgD3D_AfterReinitialise(param);
	RecalculateUI();
}

void D3DViewport_Set(D3DViewport* viewport, int left, int top, int right, int bottom)
{
	if (viewport == nullptr)
	{
		viewport = *gDefaultViewport;
	}

	const int32_t ResWidth = GetResolutionWidth();
	const int32_t ResHeight = GetResolutionHeight();

	viewport->m_left = left;
	viewport->m_top = top;
	viewport->m_right = right;
	viewport->m_bottom = bottom;
	viewport->m_horFov = (4.0f * bottom) / (3.0f * right);
	viewport->m_leftScale = static_cast<float>(left) / ResWidth;
	viewport->m_topScale = static_cast<float>(top) / ResHeight;
	viewport->m_rightScale = static_cast<float>(right) / ResWidth;
	viewport->m_bottomScale = static_cast<float>(bottom) / ResHeight;
	viewport->m_vertFov = 4.0f/3.0f;
}

void D3DViewport_GetAspectRatioForCoDriver(D3DViewport* /*viewport*/, float* horFov, float* vertFov)
{
	*horFov = *vertFov = 1.0f;
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
