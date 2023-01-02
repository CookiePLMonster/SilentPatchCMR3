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

	if (!Version::HasMultipleLocales())
	{
		if (Version::IsPolish())
		{
			result.emplace(802, BONUSCODES_URL);
		}

		if (Version::IsCzech())
		{
			// Original Czech string for "NA" is broken, as it was unused before
			result.emplace(994, "ND");
		}
	}

	return result;
}

static std::map<uint32_t, const char*> GetNewStrings()
{
	std::map<uint32_t, const char*> result = {
		{ LANGUAGE_POLISH, "POLSKI" },
		{ LANGUAGE_CZECH, "\xC3" "ESKY" },

		{ 1008, "F1" }, { 1009, "F2" }, { 1010, "F3" }, { 1011, "F4" },
		{ 1012, "F5" }, { 1013, "F6" }, { 1014, "F7" }, { 1015, "F8" },
		{ 1016, "F9" }, { 1017, "F10" }, { 1018, "F11" }, { 1019, "F12" },

		{ 1033, "[?]" },
	};

	if (Version::IsPolish())
	{
		result.merge(std::map<uint32_t, const char*> {
			{ 994, "ND" },
			{ 1004, "ENTER" }, { 1005, "ESC" }, { 1006, "BACKSPACE" }, { 1007, "TAB" },
			{ 1020, "PRINTSCREEN" }, { 1021, "SCROLLLOCK" }, { 1022, "PAUZA" }, { 1023, "INSERT" },
			{ 1024, "HOME" }, { 1025, "PGUP" }, { 1026, "DELETE" }, { 1027, "END" },
			{ 1028, "PGDN" }, { 1029, "PRAWO" }, { 1030, "LEWO" }, { 1031, "G\xD3RA" },
			{ 1032, "D\xD3\xA3" },

			{ RETURN_TO_CENTRE, "POWR\xD3T DO POZ. NEUTRALNEJ" },

			{ CODRIVER_POLISH_A, "JANUSZ KULIG" },
			{ CODRIVER_POLISH_B, "JANUSZ WITUCH" },
			{ CODRIVER_CZECH, "CZESKI" },

			{ DISPLAY_MODE, "TRYB WY\x8CWIETLANIA" },
			{ FULLSCREEN, "PE\xA3NY EKRAN" },
			{ WINDOWED, "W OKNIE" },
			{ BORDERLESS, "BEZ RAMKI" },
			{ REFRESH_RATE, "CZ\xCAST. OD\x8CWIE\xAF" "ANIA" },
			{ VSYNC, "SYNCHRONIZACJA PION." },
			{ ANISOTROPIC, "FILTROWANIE ANIZOTROPOWE" },

			{ TACHOMETER, "OBROTOMIERZ" },
			{ EXTERIOR_FOV, "POLE WIDZENIA (ZEWN.)" },
			{ INTERIOR_FOV, "POLE WIDZENIA (WEWN.)" },
		});
	}
	else
	{
		result.merge(std::map<uint32_t, const char*> {
			{ 994, "NA" },
			{ 1004, "RETURN" }, { 1005, "ESC" }, { 1006, "BACKSPC" }, { 1007, "TAB" },
			{ 1020, "PRINTSCRN" }, { 1021, "SCROLLLOCK" }, { 1022, "PAUSE" }, { 1023, "INSERT" },
			{ 1024, "HOME" }, { 1025, "PGUP" }, { 1026, "DEL" }, { 1027, "END" },
			{ 1028, "PGDN" }, { 1029, "RIGHT" }, { 1030, "LEFT" }, { 1031, "UP" },
			{ 1032, "DOWN" },

			{ RETURN_TO_CENTRE, "RETURN TO CENTRE" },

			{ CODRIVER_POLISH_A, "POLISH (J. KULIG)" },
			{ CODRIVER_POLISH_B, "POLISH (J. WITUCH)" },
			{ CODRIVER_CZECH, "CZECH" },

			{ DISPLAY_MODE, "DISPLAY MODE" },
			{ FULLSCREEN, "FULLSCREEN" },
			{ WINDOWED, "WINDOWED" },
			{ BORDERLESS, "BORDERLESS" },
			{ REFRESH_RATE, "REFRESH RATE" },
			{ VSYNC, "VSYNC" },
			{ ANISOTROPIC, "ANISOTROPIC FILTERING" },

			{ TACHOMETER, "TACHOMETER" },
			{ EXTERIOR_FOV, "FIELD OF VIEW (CHASE)" },
			{ INTERIOR_FOV, "FIELD OF VIEW (INTERIOR)" },
		});
	}

	return result;
}

const char* Language_GetString(uint32_t ID)
{
	// Original code with bounds check added
	const LangFile* language = *gpCurrentLanguage;
	if (ID < language->m_numFiles)
	{
		static const std::map<uint32_t, const char*> overrideStrings = GetOverrideStrings();
		auto it = overrideStrings.find(ID);
		if (it != overrideStrings.end())
		{
			return it->second;
		}
		return language->m_texts[ID];
	}

	// All strings that were hardcoded in the English release, but translated in the Polish release
	// + new strings for options
	static const std::map<uint32_t, const char*> newLocalizedStrings = GetNewStrings();
	auto it = newLocalizedStrings.find(ID);
	if (it != newLocalizedStrings.end())
	{
		return it->second;
	}

	return "";
}
