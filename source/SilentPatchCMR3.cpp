#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "Utils/MemoryMgr.h"
#include "Utils/Patterns.h"

#include <cstdint>
#include <map>

namespace Localization
{
	struct LangFile
	{
		char m_magic[4]; // LANG
		uint32_t m_version;
		uint32_t m_numFiles;
		const char* m_texts[1]; // VLA
	};
	static LangFile** gActiveLanguage;
	const char* GetLocalizedString_BoundsCheck(uint32_t ID)
	{
		// Original code with bounds check added
		const LangFile* language = *gActiveLanguage;
		if (ID < language->m_numFiles)
		{
			return language->m_texts[ID];
		}

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
		};
		auto it = newLocalizedStrings.find(ID);
		if (it != newLocalizedStrings.end())
		{
			return it->second;
		}

		return "";
	}
}

void OnInitializeHook()
{
	static_assert(std::string_view(__FUNCSIG__).find("__stdcall") != std::string_view::npos, "This codebase must default to __stdcall, please change your compilation settings.");

	using namespace Memory;
	using namespace hook::txn;

	auto Protect = ScopedUnprotect::UnprotectSectionOrFullModule( GetModuleHandle( nullptr ), ".text" );

	// Added range check for localizations + new strings added in the Polish release
	try
	{
		using namespace Localization;

		auto get_localized_string = ReadCallFrom(get_pattern("E8 ? ? ? ? 8D 56 E2"));

		gActiveLanguage = *get_pattern<LangFile**>("89 0D ? ? ? ? B8 ? ? ? ? 5E", 2);
		InjectHook(get_localized_string, GetLocalizedString_BoundsCheck, PATCH_JUMP);

	}
	TXN_CATCH();
}