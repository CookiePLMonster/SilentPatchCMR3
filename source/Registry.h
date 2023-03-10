#pragma once

#include <cstdint>
#include <optional>

// Portability stuff
namespace Registry
{
	inline const wchar_t* GRAPHICS_SECTION_NAME = L"Graphics";
	inline const wchar_t* REGISTRY_SECTION_NAME = L"Registry";
	inline const wchar_t* ADVANCED_SECTION_NAME = L"Advanced";

	inline const wchar_t* DISPLAY_MODE_KEY_NAME = L"DISPLAY_MODE";
	inline const wchar_t* REFRESH_RATE_KEY_NAME = L"REFRESH_RATE";
	inline const wchar_t* VSYNC_KEY_NAME = L"VSYNC";
	inline const wchar_t* ANISOTROPIC_KEY_NAME = L"ANISOTROPIC";

	inline const wchar_t* EXTERIOR_FOV_KEY_NAME = L"EXTERIOR_FOV";
	inline const wchar_t* INTERIOR_FOV_KEY_NAME = L"INTERIOR_FOV";

	inline const wchar_t* SPLIT_SCREEN_KEY_NAME = L"SPLIT_SCREEN";
	inline const wchar_t* DIGITAL_TACHO_KEY_NAME = L"DIGITAL_TACHO";

	inline const wchar_t* ENVMAP_SKY_KEY_NAME = L"ENVMAP_SKY";
	inline const wchar_t* ANALOG_MENU_NAV_KEY_NAME = L"ANALOG_MENU_NAV";
	inline const wchar_t* SHARPER_SHADOWS_KEY_NAME = L"SHARPER_SHADOWS";
	inline const wchar_t* NO_TELEPORTS_KEY_NAME = L"FREEROAM";

	inline const wchar_t* SKIP_WARNING_KEY_NAME = L"SKIP_VERSION_WARNING";

	bool Init();

	void* GetInstallString_Portable(const char* subkey, const char* value);

	std::optional<uint32_t> GetRegistryDword(const wchar_t* section, const wchar_t* key);
	std::optional<char> GetRegistryChar(const wchar_t* section, const wchar_t* key);

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
