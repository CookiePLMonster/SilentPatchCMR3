#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

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

struct OSD_Element
{
	int field_0;
	std::byte gap4[20];
	int field_18;
	int field_1C;
	std::byte gap20[8];
	int field_28;
	int field_2C;
	int m_posX;
	int m_posY;
	int m_width;
	int m_height;
	int field_40;
	int field_44;
	int field_48;
	std::byte gap4C[4];
	int (*m_drawFunc)(int, int);
	int field_54;
	std::byte gap58[4];
	int field_5C;
};
static_assert(sizeof(OSD_Element) == 0x60, "Wrong size: OSD_Element");

struct Object_StartLight
{
	int m_posX;
	int m_posY;
	int m_width;
	int m_height;
	int field_10;
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
	std::byte gap40[28];
};
static_assert(sizeof(Object_StartLight) == 0x5C, "Wrong size: Object_StartLight");


OSD_Element* OSD_Element_Init_Center(OSD_Element* element, int posX, int posY, int width, int height, int a6, int a7, int a8, int a9, int a10, int a11);
OSD_Element* OSD_Element_Init_RightAlign(OSD_Element* element, int posX, int posY, int width, int height, int a6, int a7, int a8, int a9, int a10, int a11);
OSD_Element* OSD_Element_Init(OSD_Element* element, int posX, int posY, int width, int height, int a6, int a7, int a8, int a9, int a10, int a11);

inline void (*D3DViewport_SetAspectRatio)(D3DViewport* viewport, float hfov, float vfov);
inline uint32_t (*GetResolutionWidth)();
inline uint32_t (*GetResolutionHeight)();

inline void (*Core_Blitter2D_Rect2D_G)(float* data, uint32_t numRectangles);
inline void (*Core_Blitter2D_Quad2D_GT)(float* data, uint32_t numRectangles);
inline void (*Core_Blitter2D_Line2D_G)(float* data, uint32_t numLines);

inline void (*HandyFunction_Draw2DBox)(int posX, int posY, int width, int height, int color);
inline void (*CMR3Font_BlitText)(uint8_t a1, const char* text, int posX, int posY, int a5, char a6);

inline int32_t (*GetNumPlayers)();

inline D3DViewport** gViewports;
inline D3DViewport** gDefaultViewport;

float GetScaledResolutionWidth();

void Core_Blitter2D_Rect2D_G_Center(float* data, uint32_t numRectangles);
void Core_Blitter2D_Line2D_G_Center(float* data, uint32_t numLines);
void Core_Blitter2D_Quad2D_GT_RightAlign(float* data, uint32_t numRectangles);

void HandyFunction_Draw2DBox_Stretch(int posX, int posY, int width, int height, int color);
void HandyFunction_Draw2DBox_RightAlign(int posX, int posY, int width, int height, int color);
void CMR3Font_BlitText_Center(uint8_t a1, const char* text, int posX, int posY, int a5, char a6);
void CMR3Font_BlitText_RightAlign(uint8_t a1, const char* text, int posX, int posY, int a5, char a6);

void Graphics_Viewports_SetAspectRatios();

void OSD_Main_SetUpStructsForWidescreen();

void D3D_Initialise_RecalculateUI(void* param);
void D3D_AfterReinitialise_RecalculateUI(void* param);

void D3DViewport_Set(D3DViewport* viewport, int left, int top, int right, int bottom);	

void D3DViewport_GetAspectRatioForCoDriver(D3DViewport *viewport, float* horFov, float* vertFov);

void SetMovieDirectory_SetDimensions(const char* path);

namespace Graphics::Patches
{
	inline float* UI_resolutionWidthMult;

	inline int32_t* UI_CoutdownPosXVertical[2]; // Special cased, as it's half-center

	inline int32_t* UI_MenuBarTextDrawLimit;

	// Movie rendering - special cased, scretch to fill (preserving aspect ratio)
	inline float* UI_MovieX1;
	inline float* UI_MovieY1;
	inline float* UI_MovieX2;
	inline float* UI_MovieY2;

	inline OSD_Data* orgOSDData;
	inline OSD_Data2* orgOSDData2;
	inline Object_StartLight* orgStartLightData;

	using Int32Patch = std::pair<int32_t*, int32_t>;
	using FloatPatch = std::pair<float*, float>;
	inline std::vector<std::variant<Int32Patch, FloatPatch>> UI_CenteredElements;
	inline std::vector<std::variant<Int32Patch, FloatPatch>> UI_RightAlignElements;
}