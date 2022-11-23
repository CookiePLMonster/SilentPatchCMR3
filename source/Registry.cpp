#include "Registry.h"

#include <filesystem>
#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <wil/win32_helpers.h>

static std::wstring pathToIni = L".\\SilentPatchCMR3.ini";
static const wchar_t* REGISTRY_SECTION_NAME = L"Registry";

static std::wstring AnsiToWchar(std::string_view text)
{
	std::wstring result;

	const int count = MultiByteToWideChar(CP_ACP, 0, text.data(), text.size(), nullptr, 0);
	if ( count != 0 )
	{
		result.resize(count);
		MultiByteToWideChar(CP_ACP, 0, text.data(), text.size(), result.data(), count);
	}

	return result;
}

void Registry::Init()
{
	// Set the INI path to SilentPatchCMR3.ini
	wil::unique_cotaskmem_string pathToAsi;
	if (SUCCEEDED(wil::GetModuleFileNameW(wil::GetModuleInstanceHandle(), pathToAsi)))
	{
		pathToIni = std::filesystem::path(pathToAsi.get()).replace_extension(L"ini").wstring();
	}
}

uint32_t Registry::GetRegistryDword(const char* /*subkey*/, const char* key)
{
	return GetPrivateProfileIntW(REGISTRY_SECTION_NAME, AnsiToWchar(key).c_str(), 0, pathToIni.c_str());
}

char Registry::GetRegistryChar(const char* /*subkey*/, const char* key)
{
	wchar_t buf[16];
	GetPrivateProfileStringW(REGISTRY_SECTION_NAME, AnsiToWchar(key).c_str(), L"", buf, static_cast<DWORD>(std::size(buf)), pathToIni.c_str());
	return static_cast<char>(buf[0]);
}

void Registry::SetRegistryDword(const char* /*subkey*/, const char* key, uint32_t value)
{
	WritePrivateProfileStringW(REGISTRY_SECTION_NAME, AnsiToWchar(key).c_str(), std::to_wstring(value).c_str(), pathToIni.c_str());
}

void Registry::SetRegistryChar(const char* /*subkey*/, const char* key, char value)
{
	const wchar_t buf[2] { static_cast<wchar_t>(value), '\0' };
	WritePrivateProfileStringW(REGISTRY_SECTION_NAME, AnsiToWchar(key).c_str(), buf, pathToIni.c_str());
}

void* Registry::GetInstallString_Portable(const char* /*subkey*/, const char* /*key*/)
{
	wil::unique_cotaskmem_string pathToExe;
	if (SUCCEEDED(wil::GetModuleFileNameW(nullptr, pathToExe)))
	{
		const auto pathToGameDir = std::filesystem::path(pathToExe.get()).parent_path().string();
		void* mem = Patches::orgOperatorNew(pathToGameDir.length() + 1);
		if (mem != nullptr)
		{
			char* buf = static_cast<char*>(mem);
			memcpy(buf, pathToGameDir.c_str(), pathToGameDir.length() + 1);
		}
		return mem;
	}

	return nullptr;
}
