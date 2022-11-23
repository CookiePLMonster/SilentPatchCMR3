#pragma once

#include <cstdint>

// Portability stuff
namespace Registry
{
	void Init();

	uint32_t GetRegistryDword(const char* subkey, const char* key);
	char GetRegistryChar(const char* subkey, const char* key);

	void SetRegistryDword(const char* subkey, const char* key, uint32_t value);
	void SetRegistryChar(const char* subkey, const char* key, char value);

	void* GetInstallString_Portable(const char* subkey, const char* value);

	namespace Patches
	{
		inline void* (__cdecl *orgOperatorNew)(size_t size);
	}
}