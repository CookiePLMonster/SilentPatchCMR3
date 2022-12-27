#include "Globals.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <wil/win32_helpers.h>

std::filesystem::path GetPathToGameDir()
{
    std::filesystem::path result;

	wil::unique_cotaskmem_string pathToExe;
	if (SUCCEEDED(wil::GetModuleFileNameW(nullptr, pathToExe)))
	{
		try
		{
			result = std::filesystem::path(pathToExe.get()).parent_path();
		}
		catch (const std::filesystem::filesystem_error&)
		{
		}
	}
    return result;
}
