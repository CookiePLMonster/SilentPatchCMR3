#include "Version.h"

#include "Globals.h"
#include "Registry.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <Commctrl.h>
#include <shellapi.h>

#include <filesystem>
#include <string>

#include <wil/win32_helpers.h>

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "Comctl32.lib")

#define EXE_DELTAS_DL_LINK "https://cookieplmonster.github.io/cmr3-patching-executables/"

static bool NickyGristFilesPresent = false;
static bool JanuszWituchVoiceUsed = false;
bool HasMulti7BootScreens = false, HasMulti7Locales = false, HasMulti7CoDrivers = false;
static void DetectGameFilesStuff()
{
	const std::filesystem::path pathToGame = GetPathToGameDir();
	std::error_code ec;

	// Nicky Grist files - let's check a few top files from the directory only
	{
		const wchar_t* filesToCheck[] = {
			L"Data/Sounds/Nicky/Aus2nd.coc",
			L"Data/Sounds/Nicky/Aus11.big",
			L"Data/Sounds/Nicky/AUS11.coc"
		};

		bool gristFilesPresent = true;
		for (const wchar_t* file : filesToCheck)
		{
			if (!std::filesystem::exists(pathToGame / file, ec) || ec)
			{
				gristFilesPresent = false;
				break;
			}
		}
		NickyGristFilesPresent = gristFilesPresent;
	}

	// Check for a re-release Polish co-driver
	if (Version::IsPolish())
	{
		std::uintmax_t fileSize = std::filesystem::file_size(pathToGame / L"Data/Sounds/cod/co_fre.big", ec);
		if (ec) // Error
		{
			fileSize = 0;
		}

		JanuszWituchVoiceUsed = fileSize == 1665024u;
	}

	// Multi7 boot screens - check always as we might have it as part of the locale pack or HD UI
	{
		const wchar_t* pathsToCheck[] = {
			L"Data/Boot/Polish",
			L"Data/Boot/Czech",
			L"Data/Boot/English",
			L"Data/Boot/French",
			L"Data/Boot/Spanish",
			L"Data/Boot/German",
			L"Data/Boot/Italian",	
		};
		const wchar_t* filesToCheck[] = {
			L"Copy001.dds", L"Copy002.dds", L"Copy003.dds"
		};
		
		bool bootFilesPresent = true;
		for (const wchar_t* path : pathsToCheck)
		{
			const std::filesystem::path currentPath = pathToGame / path;
			for (const wchar_t* file : filesToCheck)
			{
				if (!std::filesystem::exists(currentPath / file, ec) || ec)
				{
					bootFilesPresent = false;
					break;
				}
			}
		}
		HasMulti7BootScreens = bootFilesPresent;
	}

	// Multi7 texts
	{
		const wchar_t* filesToCheck[] = {
			L"Data/strings/Whole_C.Lng",
			L"Data/strings/Whole_E.Lng",
			L"Data/strings/Whole_F.Lng",
			L"Data/strings/Whole_G.Lng",
			L"Data/strings/Whole_I.Lng",
			L"Data/strings/Whole_P.Lng",
			L"Data/strings/Whole_S.Lng",

			L"Data/frontend/CreditsC.txt",
			L"Data/frontend/CreditsE.txt",
			L"Data/frontend/CreditsF.txt",
			L"Data/frontend/CreditsG.txt",
			L"Data/frontend/CreditsI.txt",
			L"Data/frontend/CreditsP.txt",
			L"Data/frontend/CreditsS.txt",
		};

		bool textsPresent = true;
		for (const wchar_t* file : filesToCheck)
		{
			if (!std::filesystem::exists(pathToGame / file, ec) || ec)
			{
				textsPresent = false;
				break;
			}
		}
		HasMulti7Locales = textsPresent;
	}

	// Multi7 co-drivers
	{
		const wchar_t* filesToCheck[] = {
			L"Data/Sounds/cod/co_cze.big",
			L"Data/Sounds/cod/co_fre.big",
			L"Data/Sounds/cod/co_ger.big",
			L"Data/Sounds/cod/co_ita.big",
			L"Data/Sounds/cod/co_pola.big",
			L"Data/Sounds/cod/co_polb.big",
			L"Data/Sounds/cod/co_spa.big",
		};

		bool coDriversPresent = true;
		for (const wchar_t* file : filesToCheck)
		{
			if (!std::filesystem::exists(pathToGame / file, ec) || ec)
			{
				coDriversPresent = false;
				break;
			}
		}
		HasMulti7CoDrivers = coDriversPresent;
	}
}


namespace Version
{

bool DetectVersion(const bool HasRegistry)
{
	const HMODULE gameModule = GetModuleHandle(nullptr);

	PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(gameModule);
	PIMAGE_NT_HEADERS ntHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<char*>(dosHeader) + dosHeader->e_lfanew);

	ExecutableVersion = (static_cast<uint64_t>(ntHeader->FileHeader.TimeDateStamp) << 32) | ntHeader->OptionalHeader.SizeOfImage;

	DetectGameFilesStuff();

	// If an unknown executable version is used, display a warning; else, just quit
	if (IsSupportedVersion())
	{
		return true;
	}

	// "Do not ask again" was selected in the past
	if (HasRegistry && Registry::GetRegistryDword(Registry::ADVANCED_SECTION_NAME, Registry::SKIP_WARNING_KEY_NAME) == 1)
	{
		return true;
	}

	auto fnDialogFunc = [] (HWND hwnd, UINT msg, WPARAM, LPARAM lParam, LONG_PTR) -> HRESULT
	{
		if (msg == TDN_CREATED)
		{
			const HMODULE gameModule = GetModuleHandle(nullptr);
			if (HICON icon = LoadIcon(gameModule, MAKEINTRESOURCE(101)); icon != nullptr)
			{
				SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
			}
			else
			{
				wil::unique_cotaskmem_string pathToExe;
				if (SUCCEEDED(wil::GetModuleFileNameW(gameModule, pathToExe)))
				{
					icon = ExtractIconW(gameModule, std::filesystem::path(pathToExe.get()).replace_filename(L"Rally_3PC.ico").c_str(), 0);
					if (icon != nullptr)
					{
						SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
					}
				}
			}
		}
		if (msg == TDN_HYPERLINK_CLICKED)
		{
			if (reinterpret_cast<LPCWSTR>(lParam) == std::wstring_view(L"dl-link"))
			{
				ShellExecute(nullptr, TEXT("open"), TEXT(EXE_DELTAS_DL_LINK), nullptr, nullptr, SW_SHOW);
				exit(0);
			}
		}

		return S_OK;
	};

	TASKDIALOGCONFIG dialogConfig { sizeof(dialogConfig) };
	dialogConfig.dwFlags = TDF_CAN_BE_MINIMIZED|TDF_ENABLE_HYPERLINKS;
	dialogConfig.dwCommonButtons = TDCBF_YES_BUTTON|TDCBF_NO_BUTTON;
	dialogConfig.pszWindowTitle = L"SilentPatch";
	dialogConfig.pszContent = L"An unsupported game version was detected!\n\n"
		L"Your executable version is not supported, likely an unpatched executable or a no-CD patch. "
		L"This patch is made with DRM-free executables in mind, as they provide a better experience.\n\n"
		L"You may patch your original 1.1 executable to a DRM-free version by using a patcher. "
		L"Please check the following link for instructions on applying an official 1.1 patch, and then upgrading your game executable to a DRM-free version:\n\n"
		L"<A HREF=\"dl-link\">" EXE_DELTAS_DL_LINK L"</A>\n"
		L"(Clicking on this link will abort loading and open a web page with instructions.)\n\n"
		L"Press Yes if you wish to proceed to the game with SilentPatch loaded. However, this is unsupported "
		L"and may result in unforeseen consequences, ranging from missing fixes to new crashes.\n"
		L"Press No to proceed without SilentPatch loaded (recommended).";
	dialogConfig.nDefaultButton = IDNO;
	dialogConfig.pszMainIcon = TD_WARNING_ICON;
	dialogConfig.pfCallback = fnDialogFunc;
	if (HasRegistry)
	{
		dialogConfig.pszVerificationText = L"Do not ask again";
	}

	int buttonResult = IDNO;
	BOOL doNotAskAgain = FALSE;
	if (SUCCEEDED(TaskDialogIndirect(&dialogConfig, &buttonResult, nullptr, &doNotAskAgain)))
	{
		if (buttonResult == IDCANCEL)
		{
			// Cancelled - abort startup
			exit(0);
		}

		if (buttonResult == IDYES)
		{
			// Remember not to ask again if we can save it
			if (HasRegistry && doNotAskAgain != FALSE)
			{
				Registry::SetRegistryDword(Registry::ADVANCED_SECTION_NAME, Registry::SKIP_WARNING_KEY_NAME, 1);
			}
			return true;
		}
		return false;
	}
	else
	{
		// Error! Do not inject SP just to be safe
		return false;
	}
}

bool IsSupportedVersion()
{
	return ExecutableVersion == VERSION_EFIGS_DRMFREE || ExecutableVersion == VERSION_POLISH_DRMFREE || ExecutableVersion == VERSION_CZECH_DRMFREE;
}

bool IsEFIGS()
{
	return ExecutableVersion == VERSION_EFIGS_DRMFREE || ExecutableVersion == VERSION_EFIGS_11;
}

bool IsPolish()
{
	return ExecutableVersion == VERSION_POLISH_11 || ExecutableVersion == VERSION_POLISH_DRMFREE;
}

bool IsCzech()
{
	return ExecutableVersion == VERSION_CZECH_DRMFREE;
}

bool HasNickyGristFiles()
{
	return NickyGristFilesPresent;
}

bool HasJanuszWituchVoiceLines()
{
	return JanuszWituchVoiceUsed;
}

bool HasMultipleBootScreens()
{
	return HasMulti7BootScreens;
}

bool HasMultipleLocales()
{
	return HasMulti7Locales;
}

bool HasMultipleCoDrivers()
{
	return HasMulti7CoDrivers;
}

bool IsKnownVersion()
{
	return IsSupportedVersion() || ExecutableVersion == VERSION_EFIGS_11 || ExecutableVersion == VERSION_POLISH_11;
}

}