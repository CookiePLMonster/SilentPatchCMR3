#pragma once

#include <filesystem>

inline const char* BONUSCODES_URL = "HTTPS://COOKIEPLMONSTER.GITHUB.IO/BONUSCODES/";

inline char* gszTempString;

inline uint32_t (*GameInfo_GetTextLanguage)();
inline uint32_t (*GameInfo_GetCoDriverLanguage)();

std::filesystem::path GetPathToGameDir();
