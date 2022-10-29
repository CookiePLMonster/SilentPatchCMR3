#pragma once

#include <cstdint>
#include <cstddef>

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
	unsigned int m_flags2;
	__int16 m_curEntry;
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

void Menu_SetUpEntries_Patched(int languagesOnly);

extern MenuDefinition* gMenus;