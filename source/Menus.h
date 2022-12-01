#pragma once

#include <cstdint>
#include <cstddef>

namespace MenuID
{
	static constexpr size_t GRAPHICS_ADVANCED = 20; // Original - 20
}

namespace EntryID
{
	static constexpr size_t GRAPHICS_ADV_DRIVER = 0; // Original - 0
	static constexpr size_t GRAPHICS_ADV_RESOLUTION = 1; // Original - 1
	static constexpr size_t GRAPHICS_ADV_ZDEPTH = 2; // Original - 2
	static constexpr size_t GRAPHICS_ADV_DISPLAYMODE = 3; // NEW
	static constexpr size_t GRAPHICS_ADV_TEXTUREQUALITY = 4; // Original - 3
	static constexpr size_t GRAPHICS_ADV_ENVMAP = 5; // Original - 4
	static constexpr size_t GRAPHICS_ADV_SHADOWS = 6; // Original - 5
	static constexpr size_t GRAPHICS_ADV_DRAWDISTANCE = 7; // Original - 6
	static constexpr size_t GRAPHICS_ADV_GAMMA = 8; // Original - 7
	static constexpr size_t GRAPHICS_ADV_FSAA = 9; // Original - 8
	static constexpr size_t GRAPHICS_ADV_ACCEPT = 10; // Original - 9
	static constexpr size_t GRAPHICS_ADV_CANCEL = 11; // Original - 10
	static constexpr size_t GRAPHICS_ADV_NUM = 12; // Original - 11
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

inline void (*SetUseLowQualityTextures)(uint32_t);

struct MenuDefinition;

#pragma warning(push)
#pragma warning(disable: 4201)
struct MenuEntry
{
	union {
		struct {
			uint32_t _pad2 : 24;
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

uint32_t PC_GraphicsAdvanced_ComputePresetQuality();
void PC_GraphicsAdvanced_SetGraphicsFromPresetQuality();

void PC_GraphicsAdvanced_LoadSettings();
void PC_GraphicsAdvanced_SaveSettings();

inline void (*PC_GraphicsAdvanced_PopulateFromCaps)(MenuDefinition* menu, uint32_t currentAdapter, uint32_t newAdapter);

void PC_GraphicsAdvanced_Enter_NewOptions(MenuDefinition* menu, int a2);
MenuDefinition* PC_GraphicsAdvanced_Select_NewOptions(MenuDefinition* menu, MenuEntry* entry);

void PC_GraphicsAdvanced_Display_NewOptions(MenuDefinition* menu, float interp, uint32_t posY, uint32_t entryID);

inline int* gnCurrentAdapter;

extern MenuDefinition* gmoFrontEndMenus;

namespace Menus::Patches
{
	inline bool ExtraAdvancedGraphicsOptionsPatched = false;
}