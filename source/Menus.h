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
	static constexpr size_t GRAPHICS_ADV_FSAA = 8; // Original - 8
}

struct MenuDefinition;

struct MenuEntry
{
	//uint32_t m_visibilityAndName;
	uint32_t _pad2 : 24;
	uint32_t m_canBeSelected : 1;
	uint32_t m_isDisplayed : 1;
	uint32_t m_wrapsValues : 1;

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

void FrontEndMenuSystem_SetupMenus_Custom(int languagesOnly);

extern MenuDefinition* gmoFrontEndMenus;

namespace Menus::Patches
{
	inline bool ExtraAdvancedGraphicsOptionsPatched = false;
}