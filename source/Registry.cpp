#include "Registry.h"

#include <filesystem>
#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <wil/win32_helpers.h>

static std::wstring pathToIni = L".\\SilentPatchCMR3.ini";

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

uint32_t Registry::GetRegistryDword(const wchar_t* section, const wchar_t* key)
{
	return GetPrivateProfileIntW(section, key, 0, pathToIni.c_str());
}

char Registry::GetRegistryChar(const wchar_t* section, const wchar_t* key)
{
	wchar_t buf[16];
	GetPrivateProfileStringW(section, key, L"", buf, static_cast<DWORD>(std::size(buf)), pathToIni.c_str());
	return static_cast<char>(buf[0]);
}

void Registry::SetRegistryDword(const wchar_t* section, const wchar_t* key, uint32_t value)
{
	WritePrivateProfileStringW(section, key, std::to_wstring(value).c_str(), pathToIni.c_str());
}

void Registry::SetRegistryChar(const wchar_t* section, const wchar_t* key, char value)
{
	const wchar_t buf[2] { static_cast<wchar_t>(value), '\0' };
	WritePrivateProfileStringW(section, key, buf, pathToIni.c_str());
}

uint32_t Registry::Patches::GetRegistryDword_Patch(const char* /*subkey*/, const char* key)
{
	return GetRegistryDword(REGISTRY_SECTION_NAME, AnsiToWchar(key).c_str());
}

char Registry::Patches::GetRegistryChar_Patch(const char* /*subkey*/, const char* key)
{
	return GetRegistryChar(REGISTRY_SECTION_NAME, AnsiToWchar(key).c_str());
}

void Registry::Patches::SetRegistryDword_Patch(const char* /*subkey*/, const char* key, uint32_t value)
{
	SetRegistryDword(REGISTRY_SECTION_NAME, AnsiToWchar(key).c_str(), value);
}

void Registry::Patches::SetRegistryChar_Patch(const char* /*subkey*/, const char* key, char value)
{
	SetRegistryChar(REGISTRY_SECTION_NAME, AnsiToWchar(key).c_str(), value);
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