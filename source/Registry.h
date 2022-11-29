#pragma once

#include <cstdint>

// Portability stuff
namespace Registry
{
	inline const wchar_t* GRAPHICS_SECTION_NAME = L"Graphics";
	inline const wchar_t* REGISTRY_SECTION_NAME = L"Registry";

	inline const wchar_t* DISPLAY_MODE_KEY_NAME = L"DISPLAY_MODE";

	void Init();

	void* GetInstallString_Portable(const char* subkey, const char* value);

	uint32_t GetRegistryDword(const wchar_t* section, const wchar_t* key);
	char GetRegistryChar(const wchar_t* section, const wchar_t* key);

	void SetRegistryDword(const wchar_t* section, const wchar_t* key, uint32_t value);
	void SetRegistryChar(const wchar_t* section, const wchar_t* key, char value);

	namespace Patches
	{
		inline void* (__cdecl *orgOperatorNew)(size_t size);

		uint32_t GetRegistryDword_Patch(const char* subkey, const char* key);
		char GetRegistryChar_Patch(const char* subkey, const char* key);

		void SetRegistryDword_Patch(const char* subkey, const char* key, uint32_t value);
		void SetRegistryChar_Patch(const char* subkey, const char* key, char value);
	}
}