#include "Version.h"

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

namespace Version
{

bool DetectVersion(const bool HasRegistry)
{
	const HMODULE gameModule = GetModuleHandle(nullptr);

	PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(gameModule);
	PIMAGE_NT_HEADERS ntHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<char*>(dosHeader) + dosHeader->e_lfanew);

	ExecutableVersion = (static_cast<uint64_t>(ntHeader->FileHeader.TimeDateStamp) << 32) | ntHeader->OptionalHeader.SizeOfImage;

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

bool IsKnownVersion()
{
	return IsSupportedVersion() || ExecutableVersion == VERSION_EFIGS_11 || ExecutableVersion == VERSION_POLISH_11;
}

}