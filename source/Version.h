#pragma once

#include <cstdint>

namespace Version
{
	static constexpr const uint64_t VERSION_EFIGS_DRMFREE = 0x415056EF0058C000u;
	static constexpr const uint64_t VERSION_POLISH_DRMFREE = 0x421CEBFA0058C000u;
	static constexpr const uint64_t VERSION_CZECH_DRMFREE = 0x413488EA0058D000u;

	static constexpr const uint64_t VERSION_EFIGS_10 = 0x3EC122FE00688000u; // Unsupported
	static constexpr const uint64_t VERSION_EFIGS_11 = 0x3F200A7F00696000u; // Unsupported
	static constexpr const uint64_t VERSION_POLISH_11 = 0x3FBCF1F900594000u; // Unsupported

	static constexpr const uint64_t VERSION_DEMO_095 = 0x3EAEB32400586000u; // Unsupported
	static constexpr const uint64_t VERSION_DEMO_10 = 0x3EDB4C4D0058A000u; // Unsupported

	inline uint64_t ExecutableVersion;

	bool DetectVersion(bool HasRegistry);

	bool IsKnownVersion();
	bool IsSupportedVersion();

	bool IsEFIGS();
	bool IsDemo();
	bool IsPolish();
	bool IsCzech();

	bool HasNickyGristFiles();

	// Polish re-release co-driver
	bool HasJanuszWituchVoiceLines();

	// Locale pack stuff
	// Multi7 boot screen (from the locale pack or HD UI)
	bool HasMultipleBootScreens();
	bool HasMultipleLocales();
	bool HasMultipleCoDrivers();

	// Guesstimate only, don't use to make any meaningful decisions! This is only for the UI
	bool HasHDUI();
}