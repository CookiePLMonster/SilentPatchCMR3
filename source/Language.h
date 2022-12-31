#pragma once

#include <cstdint>

struct LangFile;

namespace Language
{
	// Make sure those don't overlap with international OR Polish strings
	static constexpr uint32_t FIRST_SP_STRING = 1034;

	static constexpr uint32_t RETURN_TO_CENTRE = FIRST_SP_STRING+0;

	static constexpr uint32_t LANGUAGE_POLISH = FIRST_SP_STRING+1;
	static constexpr uint32_t LANGUAGE_CZECH = FIRST_SP_STRING+2;

	static constexpr uint32_t CODRIVER_POLISH_A = FIRST_SP_STRING+3;
	static constexpr uint32_t CODRIVER_POLISH_B = FIRST_SP_STRING+4;
	static constexpr uint32_t CODRIVER_CZECH = FIRST_SP_STRING+5;

	static constexpr uint32_t DISPLAY_MODE = FIRST_SP_STRING+6;
	static constexpr uint32_t FULLSCREEN = FIRST_SP_STRING+7;
	static constexpr uint32_t WINDOWED = FIRST_SP_STRING+8;
	static constexpr uint32_t BORDERLESS = FIRST_SP_STRING+9;

	static constexpr uint32_t REFRESH_RATE = FIRST_SP_STRING+10;
	static constexpr uint32_t VSYNC = FIRST_SP_STRING+11;
	static constexpr uint32_t ANISOTROPIC = FIRST_SP_STRING+12;

	static constexpr uint32_t TACHOMETER = FIRST_SP_STRING+13;
	static constexpr uint32_t EXTERIOR_FOV = FIRST_SP_STRING+14;
	static constexpr uint32_t INTERIOR_FOV = FIRST_SP_STRING+15;
}

const char* Language_GetString(uint32_t ID);

inline LangFile** gpCurrentLanguage;
