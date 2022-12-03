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
		{ 1004, "Return" }, { 1005, "Esc" }, { 1006, "BackSpc" }, { 1007, "TAB" },
		{ 1008, "F1" }, { 1009, "F2" }, { 1010, "F3" }, { 1011, "F4" },
		{ 1012, "F5" }, { 1013, "F6" }, { 1014, "F7" }, { 1015, "F8" },
		{ 1016, "F9" }, { 1017, "F10" }, { 1018, "F11" }, { 1019, "F12" },
		{ 1020, "PrintScrn" }, { 1021, "ScrollLock" }, { 1022, "Pause" }, { 1023, "Insert" },
		{ 1024, "Home" }, { 1025, "PgUp" }, { 1026, "Del" }, { 1027, "End" },
		{ 1028, "PgDn" }, { 1029, "Right" }, { 1030, "Left" }, { 1031, "Up" },
		{ 1032, "Down" }, { 1033, "[?]" },

		{ DISPLAY_MODE, "Display Mode" },
		{ FULLSCREEN, "Fullscreen" },
		{ WINDOWED, "Windowed" },
		{ BORDERLESS, "Borderless" },
		{ VSYNC, "VSync" },
		{ ANISOTROPIC, "Anisotropic Filtering" },
	};
	auto it = newLocalizedStrings.find(ID);
	if (it != newLocalizedStrings.end())
	{
		return it->second;
	}

	return "";
}
