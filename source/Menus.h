#pragma once

#include <cstdint>
#include <cstddef>

namespace MenuID
{
	static constexpr size_t GRAPHICS = 19; // Original - 19
	static constexpr size_t GRAPHICS_ADVANCED = 20; // Original - 20
	static constexpr size_t LANGUAGE = 29; // Original - 29
}

namespace EntryID
{
	static constexpr size_t GRAPHICS_ADV_DRIVER = 0; // Original - 0
	static constexpr size_t GRAPHICS_ADV_RESOLUTION = 1; // Original - 1
	static constexpr size_t GRAPHICS_ADV_ZDEPTH = 2; // Original - 2
	static constexpr size_t GRAPHICS_ADV_DISPLAYMODE = 3; // NEW
	static constexpr size_t GRAPHICS_ADV_REFRESHRATE = 4; // NEW
	static constexpr size_t GRAPHICS_ADV_VSYNC = 5; // NEW
	static constexpr size_t GRAPHICS_ADV_TEXTUREQUALITY = 6; // Original - 3
	static constexpr size_t GRAPHICS_ADV_ENVMAP = 7; // Original - 4
	static constexpr size_t GRAPHICS_ADV_SHADOWS = 8; // Original - 5
	static constexpr size_t GRAPHICS_ADV_DRAWDISTANCE = 9; // Original - 6
	static constexpr size_t GRAPHICS_ADV_ANISOTROPIC = 10; // NEW
	static constexpr size_t GRAPHICS_ADV_GAMMA = 11; // Original - 7
	static constexpr size_t GRAPHICS_ADV_FSAA = 12; // Original - 8
	static constexpr size_t GRAPHICS_ADV_ACCEPT = 13; // Original - 9
	static constexpr size_t GRAPHICS_ADV_CANCEL = 14; // Original - 10
	static constexpr size_t GRAPHICS_ADV_NUM = 15; // Original - 11

	static constexpr size_t GRAPHICS_SPLIT_SCREEN = 4; // NEW
	static constexpr size_t GRAPHICS_TACHO = 5; // NEW
	static constexpr size_t GRAPHICS_EXTERIOR_FOV = 6; // NEW
	static constexpr size_t GRAPHICS_INTERIOR_FOV = 7; // NEW
	static constexpr size_t GRAPHICS_ACCEPT = 8; // Original - 4
	static constexpr size_t GRAPHICS_BACK = 9; // Original - 5
	static constexpr size_t GRAPHICS_NUM = 10; // Original - 6
}

struct Packed_Registry
{
	std::byte _gap[32];
	uint8_t adapterID;
	uint8_t backbufferFormatID;
	uint8_t depthFormatID;
	uint8_t resolutionID;
	std::byte field_24;
};
static_assert(sizeof(Packed_Registry) == 0x25, "Wrong size: Packed_Registry");

inline uint8_t (*CMR_FE_GetTextureQuality)();
inline uint8_t (*CMR_FE_GetEnvironmentMap)();
inline uint8_t (*CMR_FE_GetGraphicsQuality)();
inline uint8_t (*CMR_FE_GetDrawDistance)();
inline uint8_t (*CMR_FE_GetDrawShadow)();

inline void (*CMR_FE_SetGraphicsQuality)(uint32_t);
inline void (*CMR_FE_SetTextureQuality)(uint32_t);
inline void (*CMR_FE_SetEnvironmentMap)(bool);
inline void (*CMR_FE_SetDrawShadow)(bool);
inline void (*CMR_FE_SetDrawDistance)(uint32_t);
inline void (*CMR_FE_SetFSAA)(uint32_t);

inline void (*CMR_FE_StoreRegistry)(const Packed_Registry* registry);

int CMR_FE_GetExteriorFOV();
int CMR_FE_GetInteriorFOV();
void CMR_FE_SetExteriorFOV(int FOV);
void CMR_FE_SetInteriorFOV(int FOV);

bool CMR_FE_GetVerticalSplitscreen();
void CMR_FE_SetVerticalSplitscreen(bool vertical);

bool CMR_FE_GetDigitalTacho();
void CMR_FE_SetDigitalTacho(bool digital);

inline void (*SetUseLowQualityTextures)(uint32_t);

struct MenuDefinition;

#pragma warning(push)
#pragma warning(disable: 4201)
struct MenuEntry
{
	union {
		struct {
			uint32_t _pad2 : 7;
			uint32_t m_displayBothOnOff : 1;
			uint32_t m_stringID : 16;
			uint32_t m_canBeSelected : 1;
			uint32_t m_isDisplayed : 1;
			uint32_t m_wrapsValues : 1;
		};
		uint32_t m_visibilityAndName;
	};

	int m_entryDataInt;
	int m_value;
	std::byte _pad1[8];
	const char* m_entryDataString;
	const char* m_description;
	MenuDefinition* m_destMenu;
	MenuDefinition* (*m_callback)(MenuDefinition*, int);
	int field_24;
};
static_assert(sizeof(MenuEntry) == 0x28, "Wrong size: MenuEntry");
#pragma warning(pop)

struct MenuDefinition
{
	MenuDefinition *m_prevMenu;
	const char *m_name;
	int field_8;
	int field_C;
	int m_flags1;
	uint32_t m_flags : 10;
	uint32_t m_numEntries : 8;
	int16_t m_curEntry;
	MenuEntry m_entries[40];
	int field_65C;
	int64_t field_660;
	int64_t field_668;
	int64_t field_670;
	int64_t field_678;
	void (__stdcall *m_funcInit)(MenuDefinition *, int);
	int (__stdcall *m_func2)(MenuDefinition *, int, int);
	void (__stdcall *m_funcDisplay)(MenuDefinition *, float);
	void (__stdcall *m_funcExit)(MenuDefinition *, int);
	void (__stdcall *m_func5)(MenuDefinition *, int);
	void (__stdcall *m_func6)(MenuDefinition *, float);
	int16_t m_curScrollTopEntry;
	int field_69C;
};
static_assert(sizeof(MenuDefinition) == 0x6A0, "Wrong size: MenuDefinition");

inline void (*DrawLeftRightArrows)(MenuDefinition* menu, uint32_t entryID, float interp, int leftArrow, int rightArrow, uint32_t posY);
void DrawLeftRightArrows_RightAlign(MenuDefinition* menu, uint32_t entryID, float interp, int leftArrow, int rightArrow, uint32_t posY);

void FrontEndMenuSystem_SetupMenus_Custom(int languagesOnly);
void ResultsMenuSystem_Initialise_Custom();

uint32_t PC_GraphicsAdvanced_ComputePresetQuality();
void PC_GraphicsAdvanced_SetGraphicsFromPresetQuality();

void PC_GraphicsAdvanced_LoadSettings();
void PC_GraphicsAdvanced_SaveSettings();

inline void (*PC_GraphicsAdvanced_PopulateFromCaps)(MenuDefinition* menu, uint32_t currentAdapter, uint32_t newAdapter);

void PC_GraphicsAdvanced_Enter_NewOptions(MenuDefinition* menu, int a2);
MenuDefinition* PC_GraphicsAdvanced_Select_NewOptions(MenuDefinition* menu, MenuEntry* entry);
void PC_GraphicsAdvanced_PopulateFromCaps_NewOptions(MenuDefinition* menu, uint32_t currentAdapter, uint32_t newAdapter);

void PC_GraphicsAdvanced_Display_NewOptions(MenuDefinition* menu, float interp, uint32_t posY, uint32_t entryID, uint32_t offColor, uint32_t onColor);

void PC_GraphicsOptions_Display_NewOptions(MenuDefinition* menu, float interp, uint32_t posY, uint32_t entryID, uint32_t offColor, uint32_t onColor);
void PC_GraphicsOptions_Enter_NewOptions(MenuDefinition* menu, int a2);
void PC_GraphicsOptions_Exit_NewOptions(MenuDefinition* menu, int a2);

inline int* gnCurrentAdapter;

inline MenuDefinition* gmoFrontEndMenus;
inline MenuDefinition* gmoResultsMenus;
//inline MenuDefinition** gpCurrentMenu;

namespace Menus::Patches
{
	inline bool ExtraGraphicsOptionsPatched = false;
	inline bool ExtraAdvancedGraphicsOptionsPatched = false;
	inline bool MultipleTextsPatched = false;
	inline bool MultipleCoDriversPatched = false;
}