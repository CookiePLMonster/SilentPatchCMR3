-- Custom API additions START
require('vstudio')

premake.api.register {
	name = "vcpkg",
	scope = "config",
	kind = "boolean"
}
premake.api.register {
	name = "vcpkgmanifest",
	scope = "config",
	kind = "boolean"
}

premake.override(premake.vstudio.vc2010.elements, "configurationProperties", function(base, cfg)
	local calls = base(cfg)
	table.insert(calls, function(cfg)
		if cfg.vcpkg ~= nil then
			premake.w('<VcpkgEnabled>%s</VcpkgEnabled>', cfg.vcpkg)
		end
		if cfg.vcpkgmanifest ~= nil then
			premake.w('<VcpkgEnableManifest>%s</VcpkgEnableManifest>', cfg.vcpkgmanifest)
		end
	end)
	return calls
end)
-- Custom API additions END

workspace "SilentPatchCMR3"
	platforms { "Win32" }

project "SilentPatchCMR3"
	kind "SharedLib"
	targetextension ".asi"
	language "C++"
	callingconvention "stdcall"

	dofile "source/VersionInfo.lua"
	files { "**/MemoryMgr.h", "**/Patterns.*", "**/HookInit.hpp" }

	-- Enable vcpkg
	vcpkgmanifest "on"


workspace "*"
	configurations { "Debug", "Release", "Shipping" }
	location "build"

	vpaths { ["Headers/*"] = "source/**.h",
			["Sources/*"] = { "source/**.c", "source/**.cpp" },
			["Resources"] = "source/**.rc"
	}

	files { "source/*.h", "source/*.cpp", "source/resources/*.rc" }

	-- Disable exceptions in WIL
	defines { "WIL_SUPPRESS_EXCEPTIONS" }

	cppdialect "C++17"
	staticruntime "on"
	buildoptions { "/sdl" }
	warnings "Extra"

	-- Automated defines for resources
	defines { "rsc_Extension=\"%{prj.targetextension}\"",
			"rsc_Name=\"%{prj.name}\"" }

filter "configurations:Debug"
	defines { "DEBUG" }
	runtime "Debug"

 filter "configurations:Shipping"
	defines { "NDEBUG", "RESULT_DIAGNOSTICS_LEVEL=0", "RESULT_INCLUDE_CALLER_RETURNADDRESS=0" }
	linkoptions { "/pdbaltpath:%_PDB%" }

filter "configurations:not Debug"
	optimize "Speed"
	functionlevellinking "on"
	flags { "LinkTimeOptimization" }

filter { "platforms:Win32" }
	system "Windows"
	architecture "x86"

filter { "platforms:Win64" }
	system "Windows"
	architecture "x86_64"

filter { "toolset:*_xp"}
	defines { "WINVER=0x0501", "_WIN32_WINNT=0x0501" } -- Target WinXP
	buildoptions { "/Zc:threadSafeInit-" }

filter { "toolset:not *_xp"}
	defines { "WINVER=0x0601", "_WIN32_WINNT=0x0601" } -- Target Win7
	conformancemode "on"
