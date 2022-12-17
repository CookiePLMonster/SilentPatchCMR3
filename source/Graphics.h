#pragma once

#define NOMINMAX
#include <d3d9.h>

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

struct Graphics_Config
{
	int m_nameHash;
	int m_resWidth;
	int m_resHeight;
	D3DFORMAT m_format;
	int m_refreshRate;
	int m_autoDepthStencil;
	int m_backBufferCount;
	int m_multiSampleType;
	int m_adapter;
	RECT m_windowRect;
	int m_windowed;
	int m_borderless;
	int field_3C;
	int field_40;
	int field_44;
	int field_48;
	int m_presentationInterval;
};
static_assert(sizeof(Graphics_Config) == 0x50, "Wrong size: Graphics_Config");

struct MenuResolutionEntry
{
	char m_displayText[64];
	int m_width;
	int m_height;
	int m_refreshRate;
	D3DFORMAT m_format;
	D3DFORMAT m_backBufferFormat;
	D3DFORMAT field_54;
	int field_58;
	int field_5C;
	int field_60;
	int field_64;
};
static_assert(sizeof(MenuResolutionEntry) == 104, "Wrong size: MenuResolutionEntry");


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
	IDirect3DSurface9 *m_pRenderTarget;
	IDirect3DSurface9 *m_pDepthStencil;
	int field_5C;
};
static_assert(sizeof(D3DViewport) == 0x60, "Wrong size: D3DViewport");

struct D3DTexture
{
	D3DTexture *m_next;
	uint32_t m_width;
	uint32_t m_height;
	D3DFORMAT m_format;
	uint32_t m_usage;
	D3DPOOL m_pool;
	IDirect3DTexture9 *m_pTexture;
	std::byte gap1C[12];
	uint32_t m_levels;
	std::byte gap2C[24];
	int m_flags;
	int field_48;
	char m_name[16];
	std::byte gap5C[4];
	int field_60;
	uint32_t m_mipFilter;
	uint32_t m_magFilter;
	uint32_t m_minFilter;
	std::byte gap70[52];
};
static_assert(sizeof(D3DTexture) == 0xA4, "Wrong size: D3DTexture");

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
	int m_TachoX;
	int m_TachoY;
	std::byte gap8;
	char field_9;
	std::byte gapA[10];
	int field_14;
	int field_18;
	int field_1C;
	int field_20;
	std::byte gap24[36];
	int m_SplitTimesX;
	int m_SplitTimesY;
	std::byte gap50[16];
	int m_StageProgressX;
	int m_StageProgressLineWidth;
	int unk;
	int m_StageProgressY;
	std::byte gap70[20];
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

struct BlitLine2D_G
{
	uint32_t color[2];
	float X[2];
	float Y[2];
	float Z[2];
};
static_assert(sizeof(BlitLine2D_G) == 32, "Wrong size: BlitLine2D_G");

struct BlitLine3D_G
{
	float X1, Y1, Z1, W1;
	float X2, Y2, Z2, W2;
	uint32_t color[2];
	float _gap3[2];
};
static_assert(sizeof(BlitLine3D_G) == 48, "Wrong size: BlitLine3D_G");

struct BlitTri2D_G
{
	uint32_t color[3];
	float X[3];
	float Y[3];
	float Z;
};
static_assert(sizeof(BlitTri2D_G) == 40, "Wrong size: BlitTri2D_G");

struct BlitTri3D_G
{
	float X1, Y1, Z1, W1;
	float X2, Y2, Z2, W2;
	float X3, Y3, Z3, W3;
	uint32_t color[3];
	float _gap2;

};
static_assert(sizeof(BlitTri3D_G) == 64, "Wrong size: BlitTri3D_G");

struct BlitQuad2D_G
{
	uint32_t color[4];
	float X[4];
	float Y[4];
	float Z;
};
static_assert(sizeof(BlitQuad2D_G) == 52, "Wrong size: BlitQuad2D_GT");

struct BlitQuad2D_GT
{
	uint32_t color[4];
	float U[4];
	float V[4];
	float X[4];
	float Y[4];
	float Z;
};
static_assert(sizeof(BlitQuad2D_GT) == 84, "Wrong size: BlitQuad2D_GT");

struct BlitRect2D_G
{
	uint32_t color[4];
	float X[2];
	float Y[2];
	float Z;
};
static_assert(sizeof(BlitRect2D_G) == 36, "Wrong size: BlitRect2D_G");

struct BlitRect2D_GT
{
	uint32_t color[4];
	float U[2];
	float V[2];
	float X[2];
	float Y[2];
	float Z;
};
static_assert(sizeof(BlitRect2D_GT) == 52, "Wrong size: BlitRect2D_GT");

void Core_Texture_SetFilteringMethod(D3DTexture* texture, uint32_t min, uint32_t mag, uint32_t mip);

OSD_Element* OSD_Element_Init_Center(OSD_Element* element, int posX, int posY, int width, int height, int a6, int a7, int a8, int a9, int a10, int a11);
OSD_Element* OSD_Element_Init_RightAlign(OSD_Element* element, int posX, int posY, int width, int height, int a6, int a7, int a8, int a9, int a10, int a11);
OSD_Element* OSD_Element_Init(OSD_Element* element, int posX, int posY, int width, int height, int a6, int a7, int a8, int a9, int a10, int a11);

inline void (*Viewport_SetAspectRatio)(D3DViewport* viewport, float hfov, float vfov);
inline uint32_t (*Graphics_GetScreenWidth)();
inline uint32_t (*Graphics_GetScreenHeight)();

inline void (*Core_Blitter2D_Rect2D_G)(BlitRect2D_G* rects, uint32_t numRectangles);
inline void (*Core_Blitter2D_Rect2D_GT)(BlitRect2D_GT* rects, uint32_t numRectangles);
inline void (*Core_Blitter2D_Line2D_G)(BlitLine2D_G* lines, uint32_t numLines);

inline void (*HandyFunction_Draw2DBox)(int posX, int posY, int width, int height, int color);
inline void (*HandyFunction_Draw2DLineFromTo)(float x1, float y1, float x2, float y2, uint32_t* z, uint32_t color);
inline void (*CMR3Font_BlitText)(uint8_t fontID, const char* text, int posX, int posY, int a5, char a6);
inline int (*CMR3Font_GetTextWidth)(uint8_t fontID, const char* text);

inline void (*Keyboard_DrawTextEntryBox)(int posX, int posY, int a3, int a4, uint32_t a5, int a6);

inline void (*Graphics_SetGammaRamp)(int flag, float gamma);
inline uint32_t (*Graphics_GetNumAdapters)();
inline void (*Graphics_CheckForVertexShaders)(int, int, int);
inline D3DCAPS9* (*Graphics_GetAdapterCaps)(D3DCAPS9* hdc, int index);

inline int32_t (*CMR_GetAdapterProductID)(int32_t, int32_t);
inline int32_t (*CMR_GetAdapterVendorID)(int32_t, int32_t);
inline int32_t (*CMR_GetValidModeIndex)(int32_t, int32_t, int32_t, int32_t);
inline int32_t (*CMR_ValidateModeFormats)(int32_t adapter);
inline void (*CMR_SetupRender)();

void CMR_FE_SetAnisotropicLevel(uint32_t level);
uint32_t CMR_FE_GetAnisotropicLevel();
uint32_t CMR_FE_GetMaxAnisotropicLevel(int adapter);

uint32_t CMR_GetAnisotropicLevel();

inline const MenuResolutionEntry* (*GetMenuResolutionEntry)(int32_t, int32_t);

//inline int32_t (*GetNumPlayers)();

inline Graphics_Config* gGraphicsConfig;
const Graphics_Config& Graphics_GetCurrentConfig();

inline D3DViewport** gViewports;
inline D3DViewport** gDefaultViewport;

inline D3DPRESENT_PARAMETERS* gd3dPP;

float GetScaledResolutionWidth();

void Core_Blitter2D_Rect2D_G_Center(BlitRect2D_G* rects, uint32_t numRectangles);
void Core_Blitter2D_Line2D_G_Center(BlitLine2D_G* lines, uint32_t numLines);

void Core_Blitter2D_Rect2D_GT_CenterHalf(BlitRect2D_GT* rects, uint32_t numRectangles);
void Core_Blitter2D_Rect2D_GT_RightAlign(BlitRect2D_GT* rects, uint32_t numRectangles);

void HandyFunction_Draw2DBox_Stretch(int posX, int posY, int width, int height, int color);
void HandyFunction_Draw2DBox_Center(int posX, int posY, int width, int height, int color);
void HandyFunction_Draw2DBox_RightAlign(int posX, int posY, int width, int height, int color);
void HandyFunction_Draw2DLineFromTo_Center(float x1, float y1, float x2, float y2, uint32_t* z, uint32_t color);

void CMR3Font_BlitText_Center(uint8_t a1, const char* text, int posX, int posY, int a5, char a6);
void CMR3Font_BlitText_RightAlign(uint8_t a1, const char* text, int posX, int posY, int a5, char a6);

void Keyboard_DrawTextEntryBox_Center(int posX, int posY, int a3, int a4, uint32_t a5, int a6);

uint32_t HandyFunction_AlphaCombineFlat(uint32_t color, uint32_t alpha);

void Graphics_Viewports_SetAspectRatios();

void OSD_Main_SetUpStructsForWidescreen();

void Viewport_SetDimensions(D3DViewport* viewport, int left, int top, int right, int bottom);

void SetMovieDirectory_SetDimensions(const char* path);

void RecalculateUI();

namespace Graphics::Patches
{
	inline float* UI_resolutionWidthMult;

	inline int32_t* UI_CoutdownPosXVertical[2]; // Special cased, as it's half-center

	inline int32_t* UI_MenuBarTextDrawLimit;

	inline int32_t* UI_TachoInitialised;

	// Movie rendering - special cased, scretch to fill (preserving aspect ratio)
	inline float* UI_MovieX1;
	inline float* UI_MovieY1;
	inline float* UI_MovieX2;
	inline float* UI_MovieY2;

	inline OSD_Data* orgOSDPositions;
	inline OSD_Data2* orgOSDPositionsMulti;
	inline Object_StartLight* orgStartLightData;

	using Int32Patch = std::pair<int32_t*, int32_t>;
	using FloatPatch = std::pair<float*, float>;
	inline std::vector<std::variant<Int32Patch, FloatPatch>> UI_CenteredElements;
	inline std::vector<std::variant<Int32Patch, FloatPatch>> UI_RightAlignElements;
}