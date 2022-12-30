#pragma once

#include <filesystem>

inline const char* BONUSCODES_URL = "HTTPS://COOKIEPLMONSTER.GITHUB.IO/BONUSCODES/";

static constexpr uint32_t TEXT_LANG_ENGLISH = 0;
static constexpr uint32_t TEXT_LANG_FRENCH = 1;
static constexpr uint32_t TEXT_LANG_SPANISH = 2;
static constexpr uint32_t TEXT_LANG_GERMAN = 3;
static constexpr uint32_t TEXT_LANG_ITALIAN = 4;
static constexpr uint32_t TEXT_LANG_POLISH = 5;
static constexpr uint32_t TEXT_LANG_CZECH = 6;

inline uint32_t gResolutionWidthPixels = 640, gResolutionHeightPixels = 480;

inline char* gszTempString;
inline uint8_t* gKeyboardData;

inline uint32_t (*GameInfo_GetTextLanguage)();
inline uint32_t (*GameInfo_GetCoDriverLanguage)();

uint32_t GameInfo_GetTextLanguage_LocalePackCheck();

char Keyboard_ConvertScanCodeToCharLocalised(int scanCode, int shift, int capsLock);

std::filesystem::path GetPathToGameDir();
