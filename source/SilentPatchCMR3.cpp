#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "Utils/MemoryMgr.h"
#include "Utils/Patterns.h"

void OnInitializeHook()
{
	static_assert(std::string_view(__FUNCSIG__).find("__stdcall") != std::string_view::npos, "This codebase must default to __stdcall, please change your compilation settings.");
}