#pragma once

#include <cstddef>
#include <cstdint>

struct D3DViewport
{
	int m_handle;
	int field_4;
	int field_8;
	int m_left;
	int m_top;
	int m_right;
	int m_bottom;
	float m_horFov;
	float m_vertFov;
	int field_24;
	float m_leftScale;
	float m_topScale;
	float m_rightScale;
	float m_bottomScale;
	int field_38;
	int field_3C;
	int field_40;
	std::byte gap44[16];
	struct IDirect3DSurface9 *m_pRenderTarget;
	struct IDirect3DSurface9 *m_pDepthStencil;
	int field_5C;
};
static_assert(sizeof(D3DViewport) == 0x60, "Wrong size: D3DViewport");

struct OSD_Data
{
	const char *field_0;
	float field_4;
	float field_8;
	float field_C;
	float field_10;
	int field_14;
	int field_18;
	int field_1C;
	int field_20;
	int field_24;
	int field_28;
	int field_2C;
	int field_30;
	int field_34;
	int field_38;
	int field_3C;
	std::byte gap40[8];
	int field_48;
	int field_4C;
	std::byte gap50[16];
	int field_60;
	int field_64;
	int field_68;
	int field_6C;
	std::byte gap70[76];
	int m_CoDriverX;
	int m_CoDriverY;
};
static_assert(sizeof(OSD_Data) == 0xC4, "Wrong size: OSD_Data");

struct OSD_Data2
{
	int field_0;
	int field_4;
	std::byte gap8;
	char field_9;
	std::byte gapA[10];
	int field_14;
	int field_18;
	int field_1C;
	int field_20;
	std::byte gap24[96];
	int m_CoDriverX;
	int m_CoDriverY;
};
static_assert(sizeof(OSD_Data2) == 0x8C, "Wrong size: OSD_Data2");

inline void (*D3DViewport_SetAspectRatio)(D3DViewport* viewport, float hfov, float vfov);
inline uint32_t (*GetResolutionWidth)();
inline uint32_t (*GetResolutionHeight)();

inline int32_t (*GetNumPlayers)();

inline D3DViewport** gViewports;
inline D3DViewport** gDefaultViewport;

void Graphics_Viewports_SetAspectRatios();

void OSD_Main_SetUpStructsForWidescreen();
void OSD_CoDriver_LoadGraphics();

void D3D_Initialise_RecalculateUI(void* param);
void D3D_AfterReinitialise_RecalculateUI(void* param);

void D3DViewport_Set(D3DViewport* viewport, int left, int top, int right, int bottom);	

void D3DViewport_GetAspectRatioForCoDriver(D3DViewport *viewport, float* horFov, float* vertFov);

namespace Graphics::Patches
{
	inline float* UI_resolutionWidthMult;
	inline float* UI_resolutionWidth;

	inline int32_t* UI_TachoScreenScale[2];
	inline float* UI_TachoPosX; // -50.0f

	inline int32_t* UI_CoutdownPosXHorizontal;
	inline int32_t* UI_CoutdownPosXVertical[2];

	inline int32_t* UI_MenuBarWidth;
	inline int32_t* UI_MenuBarTextDrawLimit;

	inline OSD_Data* orgOSDData;
	inline OSD_Data2* orgOSDData2;
}