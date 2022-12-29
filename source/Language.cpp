#include "Language.h"

#include "Globals.h"
#include "Version.h"

#include <map>

using namespace Language;

struct LangFile
{
	char m_magic[4]; // LANG
	uint32_t m_version;
	uint32_t m_numFiles;
	const char* m_texts[1]; // VLA
};

static std::map<uint32_t, const char*> GetOverrideStrings()
{
	std::map<uint32_t, const char*> result;

	if (!Version::HasMultipleCoDrivers() && Version::HasJanuszWituchVoiceLines())
	{
		result.emplace(447, "JANUSZ WITUCH");
	}

	if (!Version::HasMultipleLocales() && Version::IsPolish())
	{
		result.emplace(802, BONUSCODES_URL);
	}

	return result;
}

static std::map<uint32_t, const char*> GetNewStrings()
{
	std::map<uint32_t, const char*> result = {
		{ 994, "NA" },
		{ 1004, "RETURN" }, { 1005, "ESC" }, { 1006, "BACKSPC" }, { 1007, "TAB" },
		{ 1008, "F1" }, { 1009, "F2" }, { 1010, "F3" }, { 1011, "F4" },
		{ 1012, "F5" }, { 1013, "F6" }, { 1014, "F7" }, { 1015, "F8" },
		{ 1016, "F9" }, { 1017, "F10" }, { 1018, "F11" }, { 1019, "F12" },
		{ 1020, "PRINTSCRN" }, { 1021, "SCROLLLOCK" }, { 1022, "PAUSE" }, { 1023, "INSERT" },
		{ 1024, "HOME" }, { 1025, "PGUP" }, { 1026, "DEL" }, { 1027, "END" },
		{ 1028, "PGDN" }, { 1029, "RIGHT" }, { 1030, "LEFT" }, { 1031, "UP" },
		{ 1032, "DOWN" }, { 1033, "[?]" },

		{ LANGUAGE_POLISH, "POLSKI" },
		{ LANGUAGE_CZECH, "\xC3" "ESKY" },

		{ CODRIVER_POLISH_A, "POLISH (J. KULIG)" },
		{ CODRIVER_POLISH_B, "POLISH (J. WITUCH)" },
		{ CODRIVER_CZECH, "CZECH" },

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

	return result;
}

const char* Language_GetString(uint32_t ID)
{
	static const std::map<uint32_t, const char*> overrideStrings = GetOverrideStrings();
	auto it = overrideStrings.find(ID);
	if (it != overrideStrings.end())
	{
		return it->second;
	}

	// Original code with bounds check added
	const LangFile* language = *gpCurrentLanguage;
	if (ID < language->m_numFiles)
	{
		return language->m_texts[ID];
	}

	// All strings that were hardcoded in the English release, but translated in the Polish release
	// + new strings for options
	static const std::map<uint32_t, const char*> newLocalizedStrings = GetNewStrings();
	it = newLocalizedStrings.find(ID);
	if (it != newLocalizedStrings.end())
	{
		return it->second;
	}

	return "";
}
