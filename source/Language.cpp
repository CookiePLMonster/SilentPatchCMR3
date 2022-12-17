#include "Language.h"

#include <map>

struct LangFile
{
	char m_magic[4]; // LANG
	uint32_t m_version;
	uint32_t m_numFiles;
	const char* m_texts[1]; // VLA
};

const char* Language_GetString(uint32_t ID)
{
	// Original code with bounds check added
	const LangFile* language = *gpCurrentLanguage;
	if (ID < language->m_numFiles)
	{
		return language->m_texts[ID];
	}

	using namespace Language;

	// All strings that were hardcoded in the English release, but translated in the Polish release
	static const std::map<uint32_t, const char*> newLocalizedStrings = {
		{ 994, "NA" },
		{ 1004, "RETURN" }, { 1005, "ESC" }, { 1006, "BACKSPC" }, { 1007, "TAB" },
		{ 1008, "F1" }, { 1009, "F2" }, { 1010, "F3" }, { 1011, "F4" },
		{ 1012, "F5" }, { 1013, "F6" }, { 1014, "F7" }, { 1015, "F8" },
		{ 1016, "F9" }, { 1017, "F10" }, { 1018, "F11" }, { 1019, "F12" },
		{ 1020, "PRINTSCRN" }, { 1021, "SCROLLLOCK" }, { 1022, "PAUSE" }, { 1023, "INSERT" },
		{ 1024, "HOME" }, { 1025, "PGUP" }, { 1026, "DEL" }, { 1027, "END" },
		{ 1028, "PGDN" }, { 1029, "RIGHT" }, { 1030, "LEFT" }, { 1031, "UP" },
		{ 1032, "DOWN" }, { 1033, "[?]" },

		{ DISPLAY_MODE, "DISPLAY MODE" },
		{ FULLSCREEN, "FULLSCREEN" },
		{ WINDOWED, "WINDOWED" },
		{ BORDERLESS, "BORDERLESS" },
		{ VSYNC, "VSYNC" },
		{ ANISOTROPIC, "ANISOTROPIC FILTERING" },

		{ TACHOMETER, "TACHO" },
		{ EXTERIOR_FOV, "FOV (CHASE)" },
		{ INTERIOR_FOV, "FOV (INTERIOR)" },
	};
	auto it = newLocalizedStrings.find(ID);
	if (it != newLocalizedStrings.end())
	{
		return it->second;
	}

	return "";
}
