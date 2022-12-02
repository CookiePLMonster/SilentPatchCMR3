#pragma once

#include <cstdint>

struct LangFile;

namespace Language
{
	// Make sure those don't overlap with international OR Polish strings
	static constexpr uint32_t FIRST_SP_STRING = 1034;

	static constexpr uint32_t DISPLAY_MODE = FIRST_SP_STRING+0;
	static constexpr uint32_t FULLSCREEN = FIRST_SP_STRING+1;
	static constexpr uint32_t WINDOWED = FIRST_SP_STRING+2;
	static constexpr uint32_t BORDERLESS = FIRST_SP_STRING+3;

	static constexpr uint32_t VSYNC = FIRST_SP_STRING+4;
}

const char* Language_GetString(uint32_t ID);

inline LangFile** gpCurrentLanguage;
