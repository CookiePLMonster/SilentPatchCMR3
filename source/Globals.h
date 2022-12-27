#pragma once

#include <filesystem>

inline char* gszTempString;

inline uint32_t (*GameInfo_GetTextLanguage)();
inline uint32_t (*GameInfo_GetCoDriverLanguage)();

std::filesystem::path GetPathToGameDir();
