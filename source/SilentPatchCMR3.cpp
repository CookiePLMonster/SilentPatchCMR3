#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#include <timeapi.h>

#include "Utils/MemoryMgr.h"
#include "Utils/Patterns.h"

#include "Destruct.h"
#include "Globals.h"
#include "Graphics.h"
#include "Language.h"
#include "Menus.h"
#include "RenderState.h"
#include "Registry.h"
#include "Version.h"

#include <d3d9.h>
#include <wil/com.h>
#include <wil/win32_helpers.h>

#include <algorithm>
#include <array>
#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include <DirectXMath.h>

#pragma comment(lib, "winmm.lib")

static std::vector<RECT> DesktopRects;
static HICON SmallIcon;

static const RECT& GetAdapterRect(int adapter)
{
	return static_cast<size_t>(adapter) < DesktopRects.size() ? DesktopRects[adapter] : DesktopRects.front();
}

template<typename AT>
HMODULE GetModuleHandleFromAddress(AT address)
{
	HMODULE result = nullptr;
	GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<LPCTSTR>(address), &result);
	return result;
}

namespace Cubes
{
	void SetUpCubeLayouts();
}

namespace Localization
{
	static constexpr uint32_t NUM_LOCALES = 7; // After merging
	void* gpLanguageData[NUM_LOCALES];
	const char* gpszCountryInitials[NUM_LOCALES] = { "E", "F", "S", "G", "I", "P", "C" };

	const char* szCreditsFiles[NUM_LOCALES] = {
		"\\Data\\frontend\\CreditsE.txt",
		"\\Data\\frontend\\CreditsF.txt",
		"\\Data\\frontend\\CreditsS.txt",
		"\\Data\\frontend\\CreditsG.txt",
		"\\Data\\frontend\\CreditsI.txt",
		"\\Data\\frontend\\CreditsP.txt",
		"\\Data\\frontend\\CreditsC.txt",
	};

	void* gpCreditsGroups[NUM_LOCALES];
	void* gpCreditsFileData[NUM_LOCALES];

	char GetLangFromRegistry()
	{
		return Registry::GetRegistryChar(Registry::REGISTRY_SECTION_NAME, L"LANGUAGE").value_or('E');
	}

	uint32_t (*orgGetLanguageIDByCode)(char code);
	uint32_t GetCurrentLanguageID_Patched()
	{
		return orgGetLanguageIDByCode(GetLangFromRegistry());
	}

	uint8_t* gCoDriverLanguage;
	uint32_t GetCoDriverLanguage()
	{
		return *gCoDriverLanguage;
	}

	void SetCoDriverLanguage(uint32_t language)
	{
		*gCoDriverLanguage = static_cast<int8_t>(language);
	}

	static void (*orgSetUnknown)(int a1);
	static void setUnknown_AndCoDriverLanguage(int a1)
	{
		SetCoDriverLanguage(GameInfo_GetTextLanguage());

		orgSetUnknown(a1);
	}

	static void (*orgSetMeasurementSystem_Defaults)(bool bImperial);
	void SetMeasurementSystem_Defaults()
	{
		orgSetMeasurementSystem_Defaults(GameInfo_GetTextLanguage() == TEXT_LANG_ENGLISH);
	}

	template<std::size_t Index>
	static void (*orgSetMeasurementSystem)(bool bImperial);

	template<std::size_t Index>
	void SetMeasurementSystem_FromLanguage(bool)
	{
		orgSetMeasurementSystem<Index>(GameInfo_GetTextLanguage() == TEXT_LANG_ENGLISH);
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl_SetMeasurementSystem(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), orgSetMeasurementSystem<Ctr << 16 | I>, SetMeasurementSystem_FromLanguage<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach_SetMeasurementSystem(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl_SetMeasurementSystem<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}

	namespace BootScreen
	{
		static void (*orgFile_SetCurrentDirectory)(const char* path);
		void File_SetCurrentDirectory_BootScreen(const char* /*path*/)
		{			
			const std::array<const char*, 7> dirs = {
				"\\Data\\Boot\\English",
				"\\Data\\Boot\\French",
				"\\Data\\Boot\\Spanish",
				"\\Data\\Boot\\German",
				"\\Data\\Boot\\Italian",
				"\\Data\\Boot\\Polish",
				"\\Data\\Boot\\Czech"
			};

			const uint32_t langID = GameInfo_GetTextLanguage_LocalePackCheck();
			if (langID > 0 && langID < 7)
			{
				orgFile_SetCurrentDirectory(dirs[langID]);
			}
			else
			{
				orgFile_SetCurrentDirectory(dirs.front());
			}
		}
	}

	int __cdecl sprintf_cod(char* Buffer, const char* Format, const char* /*cod*/)
	{
		const char* fileName;
		switch (GameInfo_GetCoDriverLanguage())
		{
		case 1:
			fileName = "co_fre";
			break;
		case 2:
			fileName = "co_spa";
			break;
		case 3:
			fileName = "co_ger";
			break;
		case 4:
			fileName = "co_ita";
			break;
		case 5:
			fileName = "co_pola";
			break;
		case 6:
			fileName = "co_polb";
			break;
		default:
			fileName = "co_cze";
			break;
		}

		return sprintf_s(Buffer, 260, Format, fileName);
	}

	char GetLanguageCode(uint32_t langID)
	{
		if (langID < NUM_LOCALES)
		{
			return gpszCountryInitials[langID][0];
		}
		return 'E';
	}

	uint32_t GetLanguageIDByCode(char code)
	{
		auto it = std::find_if(std::begin(gpszCountryInitials), std::end(gpszCountryInitials), [code](const char* text) {
			return code == text[0];
		});
		if (it == std::end(gpszCountryInitials))
		{
			return 0;
		}
		return std::distance(std::begin(gpszCountryInitials), it);
	}

	int __cdecl sprintf_RegionalFont(char* Buffer, const char* Format, const char* fontName)
	{
		// Select what suffix to try
		char suffix;
		
		// No locale pack - hardcode specific subdirs
		if (!Version::HasMultipleLocales())
		{
			if (Version::IsPolish())
			{
				suffix = 'P';
			}
			else if (Version::IsCzech())
			{
				suffix = 'C';
			}
			else
			{
				suffix = GetLangFromRegistry();
			}
		}
		// With locale pack installed, use the current language
		else
		{
			suffix = GetLangFromRegistry();
		}

		// Try the suffixed path first
		int count = sprintf_s(Buffer, 256, "fonts\\fonts_%c\\%s.dds", suffix, fontName);

		std::error_code ec;
		if (!std::filesystem::exists(Buffer, ec) || ec)
		{
			count = sprintf_s(Buffer, 256, Format, fontName);
		}

		return count;
	}

	static uint32_t (*orgGameInfo_GetCoDriverLanguage_NoNickyGrist)();
	uint32_t GameInfo_GetCoDriverLanguage_NoNickyGrist()
	{
		return std::max(1u, orgGameInfo_GetCoDriverLanguage_NoNickyGrist()) - 1;
	}

	static void (*orgGameInfo_SetCoDriverLanguage_NoNickyGrist)(uint32_t);
	void GameInfo_SetCoDriverLanguage_NoNickyGrist(uint32_t langID)
	{
		orgGameInfo_SetCoDriverLanguage_NoNickyGrist(langID + 1);
	}

	namespace FontReloading
	{
		static void (*FrontEndFonts_Load)();
		static void (*FrontEndFonts_Destroy)();

		static std::once_flag addDestructorOnceFlag;

		template<size_t Index>
		static void (*orgDestruct_AddDestructor)(void*, void(*)());

		template<size_t Index>
		static void Destruct_AddDestructor_Once(void* group, void (*func)())
		{
			std::call_once(addDestructorOnceFlag, orgDestruct_AddDestructor<Index>, group, func);
		}

		template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
		void HookEachImpl_AddDestructor(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
		{
			(f(std::get<I>(tuple), orgDestruct_AddDestructor<Ctr << 16 | I>, Destruct_AddDestructor_Once<Ctr << 16 | I>), ...);
		}

		template<std::size_t Ctr = 0, typename Vars, typename Func>
		void HookEach_AddDestructor(Vars&& vars, Func&& f)
		{
			auto tuple = std::tuple_cat(std::forward<Vars>(vars));
			HookEachImpl_AddDestructor<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
		}

		static std::filesystem::path GetLocalizedFontsDir(uint32_t langID)
		{
			std::filesystem::path fontsDir = L"fonts\\fonts_";
			fontsDir += GetLanguageCode(langID);

			if (!std::filesystem::exists(fontsDir))
			{
				fontsDir = L"fonts";
			}
			return fontsDir;
		}

		static void (*CMR3Language_SetCurrent)(uint32_t langID);
		static void CMR3Language_SetCurrent_ReloadFonts(uint32_t langID)
		{
			// Assume we are not changing the language the very first frame of the Language screen...
			static uint32_t lastLangID = langID;

			CMR3Language_SetCurrent(langID);

			if (langID != lastLangID)
			{
				if (GetLocalizedFontsDir(langID) != GetLocalizedFontsDir(lastLangID))
				{
					FrontEndFonts_Destroy();
					FrontEndFonts_Load();
				}
				Cubes::SetUpCubeLayouts();
				lastLangID = langID;
			}
		}
	}
}

namespace Cubes
{
	D3DTexture** gBlankCubeTexture;
	D3DTexture** gGearCubeTextures;
	D3DTexture** gStageCubeTextures;

	D3DTexture** gGearCubeLayouts;
	D3DTexture** gStageCubeLayouts;

	void SetUpCubeLayouts()
	{
		const bool polishLayouts = GameInfo_GetTextLanguage() == TEXT_LANG_POLISH;
		D3DTexture* BLANK = *gBlankCubeTexture;

		// Gear cubes
		{
			const auto& automatic = gGearCubeLayouts;
			const auto& manual = automatic+3;

			D3DTexture* A = gGearCubeTextures[0];
			D3DTexture* M = gGearCubeTextures[1];
			D3DTexture* T = gGearCubeTextures[2];
			D3DTexture* R = gGearCubeTextures[4];

			if (polishLayouts && R != nullptr)
			{
				automatic[0] = A;
				automatic[1] = BLANK;
				automatic[2] = BLANK;

				manual[0] = BLANK;
				manual[1] = BLANK;
				manual[2] = R;
			}
			else
			{
				automatic[0] = A;
				automatic[1] = T;
				automatic[2] = BLANK;

				manual[0] = BLANK;
				manual[1] = M;
				manual[2] = T;
			}
		}

		// Stage cubes
		{
			const auto& stage1 = gStageCubeLayouts;
			const auto& stage2 = stage1+3;
			const auto& stage3 = stage2+3;
			const auto& stage4 = stage3+3;
			const auto& stage5 = stage4+3;
			const auto& stage6 = stage5+3;
			const auto& specialStage = stage6+3;

			D3DTexture* one = gStageCubeTextures[0];
			D3DTexture* two = gStageCubeTextures[1];
			D3DTexture* three = gStageCubeTextures[2];
			D3DTexture* four = gStageCubeTextures[3];
			D3DTexture* five = gStageCubeTextures[4];
			D3DTexture* six = gStageCubeTextures[5];
			D3DTexture* S = gStageCubeTextures[6];
			D3DTexture* O = gStageCubeTextures[7];
			D3DTexture* P = gStageCubeTextures[8];

			if (polishLayouts && (P != nullptr && O != nullptr))
			{
				stage1[0] = stage2[0] = stage3[0] = stage4[0] = stage5[0] = stage6[0] = O;
				stage1[1] = stage2[1] = stage3[1] = stage4[1] = stage5[1] = stage6[1] = S;

				specialStage[0] = P;
				specialStage[1] = O;
				specialStage[2] = S;
			}
			else
			{
				stage1[0] = stage2[0] = stage3[0] = stage4[0] = stage5[0] = stage6[0] = S;
				stage1[1] = stage2[1] = stage3[1] = stage4[1] = stage5[1] = stage6[1] = S;
				specialStage[0] = specialStage[1] = specialStage[2] = S;
			}
			stage1[2] = one;
			stage2[2] = two;
			stage3[2] = three;
			stage4[2] = four;
			stage5[2] = five;
			stage6[2] = six;
		}
	}

	template<std::size_t Index>
	static void (*orgLoadCubeTextures)();

	template<std::size_t Index>
	static void LoadCubeTextures_AndSetUpLayouts()
	{
		orgLoadCubeTextures<Index>();
		SetUpCubeLayouts();
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), orgLoadCubeTextures<Ctr << 16 | I>, LoadCubeTextures_AndSetUpLayouts<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}

	static const char* gStageCubeNames[9] = {
		"stage1", "stage2", "stage3", "stage4", "stage5", "stage6", "stageS",
		"stageO", "stageP"
	};
	static const char* gGearCubeNames[7] = {
		"gearA", "gearM", "gearT",
		"gearU", "gearR", "gearEo", "gearC"
	};

	static D3DTexture* gStageCubeTexturesSpace[9];
	static D3DTexture* gGearCubeTexturesSpace[7];

	static void (*Cubes_Destroy)();
	static void Cubes_Destroy_Custom()
	{
		for (auto& tex : gStageCubeTexturesSpace)
		{
			Texture_DestroyManaged(&tex);
		}
		for (auto& tex : gGearCubeTexturesSpace)
		{
			Texture_DestroyManaged(&tex);
		}

		Cubes_Destroy();
	}
}

namespace SecretsScreen
{
	static const char gEnglishText[] = "CALLS COST \xA3" "1 PER MINUTE. ROI: CALLS COST 1.27 EUROS PER MINUTE INC. VAT. CALLS FROM MOBILES VARY. CALLERS MUST BE OVER 16 AND HAVE PERMISSION FROM THE BILL PAYER. PRICES CORRECT AT TIME OF GOING TO PRESS.     ";
	static const char gFrenchText[] = "SERVICE CONNECTION RCB 328 223 466  COUT DE L\x27" "APPEL 0,337 EURO/MIN TTC. LE CO\xDBT DES APPELS \xC0 PARTIR D\x27N T\xC9L\xC9PHONE PORTABLE EST VARIABLE. POUR APPELER, VOUS DEVEZ AVOIR AU MOINS 16 ANS ET OBTENIR L\x27" "AUTORISATION DE LA PERSONNE PAYANT LES FACTURES DE T\xC9L\xC9PHONE. LES PRIX INDIQU\xC9S \xC9TAIENT CORRECTS AU MOMENT DE L\x27IMPRESSION.     ";
	static const char gGermanText[] = "ANRUFE KOSTEN 1.24 EURO PRO MINUTE. KOSTEN F\xDCR ANRUFE VON MOBILTELEFONEN AUS K\xD6NNEN VARIIEREN. ANRUFER M\xDCSSEN MINDESTENS 16 JAHRE ALT SEIN UND DIE ERLAUBNIS VOM ZAHLER DER TELEFONRECHNUNG EINHOLEN. PREISE WAREN ZUR ZEIT DER DRUCKLEGUNG KORREKT.     ";
	static const char gEmptyText[] = " ";

	static char gEnglishCallsTextBuffer[std::size(gEnglishText)];
	static char gFrenchCallsTextBuffer[std::size(gFrenchText)];
	static char gGermanCallsTextBuffer[std::size(gGermanText)];
	void SetUpSecretsScrollers()
	{
		if (GameInfo_GetTextLanguage() != TEXT_LANG_POLISH)
		{
			memcpy_s(gEnglishCallsTextBuffer, std::size(gEnglishCallsTextBuffer), gEnglishText, std::size(gEnglishText));
			memcpy_s(gFrenchCallsTextBuffer, std::size(gFrenchCallsTextBuffer), gFrenchText, std::size(gFrenchText));
			memcpy_s(gGermanCallsTextBuffer, std::size(gGermanCallsTextBuffer), gGermanText, std::size(gGermanText));
		}
		else
		{
			// No scroller texts
			memcpy_s(gEnglishCallsTextBuffer, std::size(gEnglishCallsTextBuffer), gEmptyText, std::size(gEmptyText));
			memcpy_s(gFrenchCallsTextBuffer, std::size(gFrenchCallsTextBuffer), gEmptyText, std::size(gEmptyText));
			memcpy_s(gGermanCallsTextBuffer, std::size(gGermanCallsTextBuffer), gEmptyText, std::size(gEmptyText));
		}
	}

	static int (*orgCMR3Font_GetTextWidth)(uint8_t fontID, const char* text);
	static int CMR3Font_GetTextWidth_Scroller(uint8_t fontID, const char* /*text*/)
	{
		SetUpSecretsScrollers();
		return orgCMR3Font_GetTextWidth(fontID, gEnglishCallsTextBuffer);
	}

	static int (*GetAccessCode)();
	static void DrawSecretsScreen(uint8_t alpha)
	{
		const bool polishSecretsScreen = GameInfo_GetTextLanguage_LocalePackCheck() == TEXT_LANG_POLISH;
		const uint32_t COLOR_WHITE = HandyFunction_AlphaCombineFlat(0xFFDCDCDC, alpha);
		const uint32_t COLOR_YELLOW = HandyFunction_AlphaCombineFlat(0xFFDCDC14, alpha);

		const int16_t SPACING_VERTICAL_SMALL = 17;
		const int16_t SPACING_VERTICAL_BIG = 26;
		const int16_t LEFT_MARGIN = 61;

		const int16_t centerPosX = static_cast<int16_t>(GetScaledResolutionWidth() / 2.0f);

		if (polishSecretsScreen)
		{
			// Polish
			int16_t posY = 60;

			CMR3Font_BlitText(0, Language_GetString(796), LEFT_MARGIN, posY, COLOR_WHITE, 33);
			posY += SPACING_VERTICAL_SMALL;

			CMR3Font_BlitText(0, BONUSCODES_URL, LEFT_MARGIN, posY, COLOR_YELLOW, 33);
			posY += SPACING_VERTICAL_SMALL;
			{
				int16_t posX = LEFT_MARGIN;
				CMR3Font_BlitText(0, Language_GetString(797), posX, posY, COLOR_WHITE, 33);
				posX += static_cast<int16_t>(CMR3Font_GetTextWidth(0, Language_GetString(797)) + CMR3Font_GetTextWidth(0, " "));

				sprintf_s(gszTempString, 512, "%.4d", GetAccessCode());
				CMR3Font_BlitText(12, gszTempString, posX, posY + 2, COLOR_YELLOW, 33);
				posX += static_cast<int16_t>(CMR3Font_GetTextWidth(0, " ") + CMR3Font_GetTextWidth(12, gszTempString));

				CMR3Font_BlitText(0, ".", posX, posY, COLOR_WHITE, 33);
			}
			posY += SPACING_VERTICAL_SMALL;
			CMR3Font_BlitText(0, Language_GetString(798), LEFT_MARGIN, posY, COLOR_WHITE, 33);
			posY += SPACING_VERTICAL_SMALL;
			CMR3Font_BlitText(0, Language_GetString(799), LEFT_MARGIN, posY, COLOR_WHITE, 33);
			posY += SPACING_VERTICAL_SMALL;
			CMR3Font_BlitText(0, Language_GetString(800), LEFT_MARGIN, posY, COLOR_WHITE, 33);

			posY = 248;
			CMR3Font_BlitText(12, Language_GetString(803), centerPosX, posY, COLOR_WHITE, 10);
			posY += SPACING_VERTICAL_BIG;
			CMR3Font_BlitText(12, Language_GetString(801), centerPosX, posY, COLOR_WHITE, 10);
		}
		else
		{
			// International

			// Top left
			int16_t posY = 60;

			CMR3Font_BlitText(0, Language_GetString(796), LEFT_MARGIN, posY, COLOR_WHITE, 33);
			posY += SPACING_VERTICAL_SMALL;
			{
				int16_t posX = LEFT_MARGIN;
				CMR3Font_BlitText(0, Language_GetString(797), posX, posY, COLOR_WHITE, 33);
				posX += static_cast<int16_t>(CMR3Font_GetTextWidth(0, Language_GetString(797)) + CMR3Font_GetTextWidth(0, " "));

				sprintf_s(gszTempString, 512, "%.4d", GetAccessCode());
				CMR3Font_BlitText(12, gszTempString, posX, posY + 2, COLOR_YELLOW, 33);
				posX += static_cast<int16_t>(CMR3Font_GetTextWidth(0, " ") + CMR3Font_GetTextWidth(12, gszTempString));

				CMR3Font_BlitText(0, ".", posX, posY, COLOR_WHITE, 33);
			}
			posY += SPACING_VERTICAL_SMALL;
			CMR3Font_BlitText(0, Language_GetString(798), LEFT_MARGIN, posY, COLOR_WHITE, 33);

			// Center
			posY = 114;

			CMR3Font_BlitText(12, Language_GetString(799), centerPosX, posY, COLOR_WHITE, 12);
			CMR3Font_BlitText(12, " 09065 558898", centerPosX, posY, COLOR_WHITE, 9);
			posY += SPACING_VERTICAL_BIG;

			CMR3Font_BlitText(12, Language_GetString(800), centerPosX, posY, COLOR_WHITE, 12);
			CMR3Font_BlitText(12, " 1570 92 30 50", centerPosX, posY, COLOR_WHITE, 9);
			posY += SPACING_VERTICAL_BIG;

			CMR3Font_BlitText(12, Language_GetString(801), centerPosX, posY, COLOR_WHITE, 12);
			CMR3Font_BlitText(12, " 08 92 69 33 77", centerPosX, posY, COLOR_WHITE, 9);
			posY += SPACING_VERTICAL_BIG;

			CMR3Font_BlitText(12, Language_GetString(802), centerPosX, posY, COLOR_WHITE, 12);
			CMR3Font_BlitText(12, " 0190 900 045", centerPosX, posY, COLOR_WHITE, 9);
	
			posY = 248;
			CMR3Font_BlitText(12, Language_GetString(803), centerPosX, posY, COLOR_WHITE, 10);
			posY += SPACING_VERTICAL_BIG;
			CMR3Font_BlitText(12, BONUSCODES_URL, centerPosX, posY, COLOR_WHITE, 10);
		}
	}

	static void CMR3Font_BlitText_NOP(uint8_t /*a1*/, const char* /*text*/, int16_t /*posX*/, int16_t /*posY*/, uint32_t /*color*/, int /*align*/)
	{
	}

	static void CMR3Font_BlitText_DrawWrapper(uint8_t /*a1*/, const char* /*text*/, int16_t /*posX*/, int16_t /*posY*/, uint32_t color, int /*align*/)
	{
		DrawSecretsScreen((color >> 24) & 0xFF);
	}
}

namespace OcclusionQueries
{
	// Divide the occlusion query results by the sample count of the backbuffer
	struct OcclusionQuery
	{
		// Stock fields, must not move
		int field_0;
		IDirect3DQuery9* m_pD3DQuery;
		DWORD m_queryData;
		uint32_t m_queryState;

		// Added in SilentPatch
		uint32_t m_sampleCount;
	};

	OcclusionQuery* (*orgOcclusionQuery_Create)();
	OcclusionQuery* OcclusionQuery_Create_SilentPatch()
	{
		OcclusionQuery* result = orgOcclusionQuery_Create();
		if (result != nullptr)
		{
			result->m_sampleCount = 0;
		}
		return result;
	}

	static bool OcclusionQuery_UpdateInternal(OcclusionQuery* query)
	{
		if (query->m_queryState == 2)
		{
			DWORD data;
			if (query->m_pD3DQuery->GetData(&data, sizeof(data), 0) == S_OK)
			{
				query->m_queryData = data;
				query->m_queryState = 3;
			}
		}
		return query->m_queryState == 3;
	}

	void OcclusionQuery_IssueBegin(OcclusionQuery* query)
	{
		query->m_pD3DQuery->Issue(D3DISSUE_BEGIN);
		query->m_queryState = 1;

		// Get the sample count
		uint32_t sampleCount = 1;

		wil::com_ptr_nothrow<IDirect3DDevice9> device;
		wil::com_ptr_nothrow<IDirect3DSurface9> depthStencil;
		if (SUCCEEDED(query->m_pD3DQuery->GetDevice(device.put())) && SUCCEEDED(device->GetDepthStencilSurface(depthStencil.put())))
		{
			D3DSURFACE_DESC desc;
			if (SUCCEEDED(depthStencil->GetDesc(&desc)))
			{
				if (desc.MultiSampleType >= D3DMULTISAMPLE_2_SAMPLES)
				{
					sampleCount = static_cast<uint32_t>(desc.MultiSampleType);
				}
			}
		}
		query->m_sampleCount = sampleCount;
	}

	void OcclusionQuery_IssueEnd(OcclusionQuery* query)
	{
		query->m_pD3DQuery->Issue(D3DISSUE_END);
		query->m_queryState = 2;
	}

	DWORD OcclusionQuery_GetDataScaled(OcclusionQuery* query)
	{	
		if (query->m_sampleCount > 1)
		{
			return query->m_queryData / query->m_sampleCount;
		}
		return query->m_queryData;
	}

	// Checks if query is completed, returns a fake state and a cached result if it's not
	uint32_t OcclusionQuery_UpdateStateIfFinished(OcclusionQuery* query)
	{
		if (!OcclusionQuery_UpdateInternal(query))
		{
			// Fake result, don't store it. OcclusionQuery_GetDataScaled will return old data
			return 3;
		}
		return query->m_queryState;
	}

	// Returns 3 when idle, 0 otherwise - to reduce the amount of assembly patches needed
	uint32_t OcclusionQuery_IsIdle(OcclusionQuery* query)
	{
		OcclusionQuery_UpdateInternal(query);
		return query->m_queryState == 0 || query->m_queryState == 3 ? 3 : 0;
	}

	void (*orgCalculateSunColorFromOcclusion)(float val);
	void CalculateSunColorFromOcclusion_Clamped(float val)
	{
		orgCalculateSunColorFromOcclusion(std::clamp(val, 0.0f, 1.0f));
	}

	static OcclusionQuery** gpSunOcclusion_Original;
	static float* gfVisibleSun_Original;

	std::array<OcclusionQuery*, 4> gapSunOcclusion;
	std::array<float, 4> gafVisibleSun;

	static OcclusionQuery* (*orgOcclusion_Create)();
	static void (*orgOcclusion_Destroy)(OcclusionQuery* occlusion);

	void* Occlusion_Create_Hooked()
	{
		for (auto& occlusion : gapSunOcclusion)
		{
			occlusion = orgOcclusion_Create();
		}

		// Return a fake result so null checks pass
		return reinterpret_cast<void*>(1);
	}

	void Occlusion_Destroy_Hooked(OcclusionQuery* /*occlusion*/)
	{
		for (auto& occlusion : gapSunOcclusion)
		{
			orgOcclusion_Destroy(occlusion);
			occlusion = nullptr;
		}
	}

	static void* Render_RenderGameCommon1_JumpBack;
	__declspec(naked) void Render_RenderGameCommon1_Original(uint8_t)
	{
		__asm
		{
			sub		esp, 20h
			push	ebx
			push	ebp
			jmp		[Render_RenderGameCommon1_JumpBack]
		}
	}

	void Render_RenderGameCommon1_SwitchOcclusion(uint8_t viewportNo)
	{
		*gpSunOcclusion_Original = gapSunOcclusion[viewportNo];
		*gfVisibleSun_Original = gafVisibleSun[viewportNo];

		Render_RenderGameCommon1_Original(viewportNo);

		gafVisibleSun[viewportNo] = *gfVisibleSun_Original;
	}
}

namespace CzechResultsScreen
{
	static int16_t savedPosY;
	
	static void (*orgHandyFunction_DrawClipped2DBox)(int posX, int posY, int width, int height, unsigned int color, int clipX, int clipY, int clipWidth, int clipHeight, int fill);
	static void HandyFunction_DrawClipped2DBox_SaveY(int posX, int posY, int width, int height, unsigned int color, int clipX, int clipY, int clipWidth, int clipHeight, int fill)
	{
		savedPosY = static_cast<int16_t>(posY - 107);
		orgHandyFunction_DrawClipped2DBox(posX, posY, width, height, color, clipX, clipY, clipWidth, clipHeight, fill);
	}

	template<std::size_t Index>
	static void (*orgCMR3Font_BlitText_SavedPos)(uint8_t fontID, const char* text, int16_t posX, int16_t posY, uint32_t color, int align);
	
	template<std::size_t Index>
	static void CMR3Font_BlitText_SavedPos(uint8_t fontID, const char* text, int16_t posX, int16_t posY, uint32_t color, int align)
	{
		if constexpr (Index == 0)
		{
			// posY is a loop index
			orgCMR3Font_BlitText_SavedPos<Index>(fontID, text, posX, 50 + posY * 44, color, align);
		}
		else if constexpr (Index == 1)
		{
			// posY is correct, just offset
			orgCMR3Font_BlitText_SavedPos<Index>(fontID, text, posX, posY - 107 - 18, color, align);
		}
		else
		{
			// posY is totally wrong
			orgCMR3Font_BlitText_SavedPos<Index>(fontID, text, posX, savedPosY, color, align);
		}
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), orgCMR3Font_BlitText_SavedPos<Ctr << 16 | I>, CMR3Font_BlitText_SavedPos<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}
}

namespace QuitMessageFix
{
	static uint32_t* gboExitProgram;

	void WINAPI PostQuitMessage_AndRequestExit(int nExitCode)
	{
		*gboExitProgram = 1;
		::PostQuitMessage(nExitCode);
	}
	auto* const pPostQuitMessage_AndRequestExit = &PostQuitMessage_AndRequestExit;
}

namespace TelemetryFadingLegend
{
	static int8_t originalWidth, originalHeight;

	void (*orgHandyFunction_Draw2DBox)(int posX, int posY, int width, int height, int color);
	void Draw2DBox_HackedAlpha(int posX, int posY, uint32_t alpha, uint32_t color)
	{
		// Parameters width and height got replaced by alpha
		orgHandyFunction_Draw2DBox(posX, posY, originalWidth, originalHeight, HandyFunction_AlphaCombineFlat(color, alpha));
	}
}

namespace CappedResolutionCountdown
{
	int __cdecl sprintf_clamp(char* Buffer, const char* Format, const char* str1, int int1, const char* str2)
	{
		return sprintf_s(Buffer, 256, Format, str1, std::max(0, int1), str2);
	}
}

namespace ConsistentControlsScreen
{
	template<std::size_t Index>
	const char* (*orgLanguage_GetString)(uint32_t ID);

	template<std::size_t Index>
	static const char* Language_GetString_Formatted(uint32_t ID)
	{
		sprintf_s(gszTempString, 512, " %s", orgLanguage_GetString<Index>(ID));
		return gszTempString;
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), orgLanguage_GetString<Ctr << 16 | I>, Language_GetString_Formatted<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}

	static int (*orgGetControllerName)(void* a1, const char** outText);
	static int GetControllerName_Uppercase(void* a1, const char** outText)
	{
		const char* text = nullptr;
		const int result = orgGetControllerName(a1, &text);

		static char stringBuffer[MAX_PATH]; // Corresponds to DirectInput's cap
		auto it =std::transform(text, text + strlen(text), stringBuffer, ::toupper);
		*it = '\0';

		*outText = stringBuffer;
		return result;
	}
}

namespace ConsistentLanguagesScreen
{
	// Also supporting multi7 now
	static const char* GetCoDriverName()
	{
		std::vector<uint32_t> coDriverNames = {
			445, 447, 449, 448, 450, Language::CODRIVER_POLISH_A, Language::CODRIVER_POLISH_B, Language::CODRIVER_CZECH
		};

		const bool excludeNickyGrist = Menus::Patches::MultipleCoDriversPatched && !Version::HasNickyGristFiles();
		if (excludeNickyGrist)
		{
			coDriverNames.erase(coDriverNames.begin());
		}

		uint32_t langID = gmoFrontEndMenus[MenuID::LANGUAGE].m_entries[1].m_value;
		if (langID >= coDriverNames.size())
		{
			return Language_GetString(coDriverNames.front());
		}
		return Language_GetString(coDriverNames[langID]);
	}

	static const char* GetLangName()
	{
		const std::array<uint32_t, 7> langNames = {
			1, 2, 4, 3, 5, Language::LANGUAGE_POLISH, Language::LANGUAGE_CZECH
		};

		uint32_t langID = gmoFrontEndMenus[MenuID::LANGUAGE].m_entries[0].m_value;
		if (langID >= langNames.size())
		{
			return Language_GetString(langNames.front());
		}
		return Language_GetString(langNames[langID]);
	}

	int __cdecl sprintf_codriver2(char* Buffer, const char* Format, const char* str1, const char* /*str2*/)
	{
		return sprintf_s(Buffer, 512, Format, str1, GetCoDriverName());
	}

	int __cdecl sprintf_codriver1(char* Buffer, const char* Format, const char* /*str*/)
	{
		return sprintf_s(Buffer, 256, Format, GetCoDriverName());
	}

	int __cdecl sprintf_lang2(char* Buffer, const char* Format, const char* str1, const char* /*str2*/)
	{
		return sprintf_s(Buffer, 512, Format, str1, GetLangName());
	}

	int __cdecl sprintf_lang1(char* Buffer, const char* Format, const char* /*str*/)
	{
		return sprintf_s(Buffer, 256, Format, GetLangName());
	}
}

namespace MoreTranslatableStrings
{
	// NA string in the Telemetry screen
	int __cdecl sprintf_na(char* Buffer, const char* /*Format*/, const char* str, int num)
	{
		return sprintf_s(Buffer, 512, "%s #%d: %s", str, num, Language_GetString(994));
	}

	// Localized keyboard key names
	static char (*Keyboard_ConvertScanCodeToChar)(int keyID, int a2, int a3);
	int Keyboard_ConvertScanCodeToString(int keyID, char* buffer)
	{
		buffer[0] = '\0';
		char keyName = Keyboard_ConvertScanCodeToChar(keyID, 0, 0);
		if (keyName != '\0')
		{
			buffer[0] = keyName;
			buffer[1] = '\0';
			return 1;
		}

		switch ( keyID )
		{
		case 1:
			strcpy_s(buffer, 256, Language_GetString(1005));
			return 1;
		case 14:
			strcpy_s(buffer, 256, Language_GetString(1006));
			return 1;
		case 15:
			strcpy_s(buffer, 256, Language_GetString(1007));
			return 1;
		case 28:
			strcpy_s(buffer, 256, Language_GetString(1004));
			return 1;
		case 59:
			strcpy_s(buffer, 256, Language_GetString(1008));
			return 1;
		case 60:
			strcpy_s(buffer, 256, Language_GetString(1009));
			return 1;
		case 61:
			strcpy_s(buffer, 256, Language_GetString(1010));
			return 1;
		case 62:
			strcpy_s(buffer, 256, Language_GetString(1011));
			return 1;
		case 63:
			strcpy_s(buffer, 256, Language_GetString(1012));
			return 1;
		case 64:
			strcpy_s(buffer, 256, Language_GetString(1013));
			return 1;
		case 65:
			strcpy_s(buffer, 256, Language_GetString(1014));
			return 1;
		case 66:
			strcpy_s(buffer, 256, Language_GetString(1015));
			return 1;
		case 67:
			strcpy_s(buffer, 256, Language_GetString(1016));
			return 1;
		case 68:
			strcpy_s(buffer, 256, Language_GetString(1017));
			return 1;
		case 70:
			strcpy_s(buffer, 256, Language_GetString(1021));
			return 1;
		case 87:
			strcpy_s(buffer, 256, Language_GetString(1018));
			return 1;
		case 88:
			strcpy_s(buffer, 256, Language_GetString(1019));
			return 1;
		case 183:
			strcpy_s(buffer, 256, Language_GetString(1020));
			return 1;
		case 197:
			strcpy_s(buffer, 256, Language_GetString(1022));
			return 1;
		case 199:
			strcpy_s(buffer, 256, Language_GetString(1024));
			return 1;
		case 200:
			strcpy_s(buffer, 256, Language_GetString(1031));
			return 1;
		case 201:
			strcpy_s(buffer, 256, Language_GetString(1025));
			return 1;
		case 203:
			strcpy_s(buffer, 256, Language_GetString(1030));
			return 1;
		case 205:
			strcpy_s(buffer, 256, Language_GetString(1029));
			return 1;
		case 207:
			strcpy_s(buffer, 256, Language_GetString(1027));
			return 1;
		case 208:
			strcpy_s(buffer, 256, Language_GetString(1032));
			return 1;
		case 209:
			strcpy_s(buffer, 256, Language_GetString(1028));
			return 1;
		case 210:
			strcpy_s(buffer, 256, Language_GetString(1023));
			return 1;
		case 211:
			strcpy_s(buffer, 256, Language_GetString(1026));
			return 1;
		default:
			strcpy_s(buffer, 256, Language_GetString(1033));
			break;
		}
		return 0;
	}

	// Localized "Return to Centre"
	int __cdecl sprintf_returntocentre1(char* Buffer, const char* Format, const char* /*str*/)
	{
		return sprintf_s(Buffer, 256, Format, Language_GetString(Language::RETURN_TO_CENTRE));
	}

	int __cdecl sprintf_returntocentre2(char* Buffer, const char* Format, const char* /*str1*/, const char* str2)
	{
		return sprintf_s(Buffer, 512, Format, Language_GetString(Language::RETURN_TO_CENTRE), str2);
	}
}

namespace UnrandomizeUnknownCodepoints
{
	int Random_RandSequence_Fixed(uint32_t)
	{
		return 0;
	}

	void CMR3Font_SetFontForBlitChar_NOP(uint8_t, uint32_t)
	{
	}
}

namespace ConditionalZWrite
{
	static bool DisableZWrite = false;
	static bool UseSharperShadows = true;

	static void* (*orgAfterSetupTextureStages)();
	static void* Graphics_CarMultitexture_AfterSetupTextureStages()
	{
		UseSharperShadows = Registry::GetRegistryDword(Registry::ADVANCED_SECTION_NAME, Registry::SHARPER_SHADOWS_KEY_NAME).value_or(1) != 0;	
		return orgAfterSetupTextureStages();
	}

	void (*orgGraphics_Shadows_Soften)(int a1);
	void Graphics_Shadows_Soften_DisableDepth(int a1)
	{
#ifndef NDEBUG
		static bool keyPressed = false;
		if (GetAsyncKeyState(VK_F4) & 0x8000)
		{
			if (!keyPressed)
			{
				UseSharperShadows = !UseSharperShadows;
				keyPressed = true;
			}
		}
		else
		{
			keyPressed = false;
		}
#endif

		DisableZWrite = true;
		orgGraphics_Shadows_Soften(UseSharperShadows ? 2 : a1);
		DisableZWrite = false;
	}

	template<std::size_t Index>
	int (*orgRenderState_SetZBufferMode)(int enable);

	template<std::size_t Index>
	int RenderState_SetZBufferMode_Conditional(int enable)
	{
		if (DisableZWrite)
		{
			return 0;
		}
		return orgRenderState_SetZBufferMode<Index>(enable);
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl_ConditionalSet(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), orgRenderState_SetZBufferMode<Ctr << 16 | I>, RenderState_SetZBufferMode_Conditional<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach_ConditionalSet(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl_ConditionalSet<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}
}

namespace FixedWaterReflections
{
	static uint32_t* reinitCountdown;

	static void (*orgSystem_RestoreAllDeviceObjects)();
	void System_RestoreAllDeviceObjects_SetCountdown()
	{
		orgSystem_RestoreAllDeviceObjects();
		*reinitCountdown = 5;
	}

	static void (*orgRenderState_Flush)();
	void RenderState_Flush_RestoreMissingStates()
	{
		orgRenderState_Flush();

		RenderStateFacade CurrentRenderStateF(CurrentRenderState);
		IDirect3DDevice9* device = *gpd3dDevice;

		// Those render states are missed by the original code
		for (DWORD i = 0; i < 8; i++)
		{
			device->SetSamplerState(i, D3DSAMP_BORDERCOLOR, CurrentRenderStateF.m_borderColor[i]);
		}

		device->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, CurrentRenderStateF.m_emissiveSource);
		device->SetRenderState(D3DRS_BLENDOP, CurrentRenderStateF.m_blendOp);
	}
}

namespace FileErrorMessages
{
	static void (*orgGraphics_Render)();
	void Graphics_Render_PumpMessages()
	{
		orgGraphics_Render();

		MSG msg;
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) != FALSE)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
			{
				exit(msg.wParam);
			}
		}
	}
}

namespace SPText
{
	static std::string BuildTextInternal()
	{
		std::string text("SilentPatch for CMR3 Build ");
		text.append(std::to_string(rsc_RevisionID));
#if rsc_BuildID > 0
		text.append(".");
		text.append(std::to_string(rsc_BuildID));
#endif
		text.append(" (" __DATE__ ")^Detected executable: ");

		switch (Version::ExecutableVersion)
		{
		case Version::VERSION_EFIGS_DRMFREE:
			text.append("1.1 International DRM-free");
			break;
		case Version::VERSION_POLISH_DRMFREE:
			text.append("1.1 Polish DRM-free");
			break;
		case Version::VERSION_CZECH_DRMFREE:
			text.append("1.1 Czech DRM-free");
			break;
		case Version::VERSION_EFIGS_10:
			text.append("1.0 International");
			break;
		case Version::VERSION_EFIGS_11:
			text.append("1.1 International");
			break;
		case Version::VERSION_POLISH_11:
			text.append("1.1 Polish");
			break;
		default:
			text.append("Unknown");
			break;
		}
		
		if (Version::IsKnownVersion() && !Version::IsSupportedVersion())
		{
			text.append(" (unsupported)");
		}

		std::vector<std::string> optionalComponents;
		if (Version::HasMultipleBootScreens() && Version::HasMultipleLocales() && Version::HasMultipleCoDrivers())
		{
			optionalComponents.emplace_back("Language Pack");
		}
		// Only mention Nicky Grist in the Polish version
		if (Version::IsPolish() && Version::HasNickyGristFiles())
		{
			optionalComponents.emplace_back("Nicky Grist Pack");
		}
		if (Version::HasHDUI())
		{
			optionalComponents.emplace_back("HD UI");
		}

		if (!optionalComponents.empty())
		{
			text.append("^Optional components: ");
			text.append(std::accumulate(std::next(optionalComponents.begin()), optionalComponents.end(), optionalComponents.front(),
				[](const std::string& a, const std::string& b) {
					return a + ", " + b;
				}));
		}

		std::string result;
		std::transform(text.begin(), text.end(), std::back_inserter(result), ::toupper);
		return result;
	}

	void DrawSPText(uint32_t color)
	{
		const uint32_t alpha = color & 0xFF000000;

		const int16_t ScreenEdge = static_cast<int16_t>(GetScaledResolutionWidth());
		const static std::string DISCLAIMER_TEXT(BuildTextInternal());

		constexpr int16_t DROP_SHADOW_WIDTH = 1;
		CMR3Font_BlitText(0, DISCLAIMER_TEXT.c_str(), ScreenEdge - 10 + DROP_SHADOW_WIDTH, 10, alpha, 4);
		CMR3Font_BlitText(0, DISCLAIMER_TEXT.c_str(), ScreenEdge - 10 - DROP_SHADOW_WIDTH, 10, alpha, 4);
		CMR3Font_BlitText(0, DISCLAIMER_TEXT.c_str(), ScreenEdge - 10, 10 + DROP_SHADOW_WIDTH, alpha, 4);
		CMR3Font_BlitText(0, DISCLAIMER_TEXT.c_str(), ScreenEdge - 10, 10 - DROP_SHADOW_WIDTH, alpha, 4);
		CMR3Font_BlitText(0, DISCLAIMER_TEXT.c_str(), ScreenEdge - 10, 10, color, 4);
	}

	template<std::size_t Index>
	void (*orgHandyFunction_BlitTexture)(void* texture, int u1, int v1, float u2, int v2, int posX, int posY, float width, int height, uint32_t color);

	template<std::size_t Index>
	void HandyFunction_BlitTexture_SPText(void* texture, int u1, int v1, float u2, int v2, int posX, int posY, float width, int height, uint32_t color)
	{
		DrawSPText(color);
		orgHandyFunction_BlitTexture<Index>(texture, u1, v1, u2, v2, posX, posY, width, height, color);
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), orgHandyFunction_BlitTexture<Ctr << 16 | I>, HandyFunction_BlitTexture_SPText<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}
}

namespace EnvMapWithSky
{
	static bool alwaysDrawSky = true;
	static void* (*orgAfterSetupTextureStages)();
	static void* Graphics_CarMultitexture_AfterSetupTextureStages()
	{
		alwaysDrawSky = Registry::GetRegistryDword(Registry::ADVANCED_SECTION_NAME, Registry::ENVMAP_SKY_KEY_NAME).value_or(1) != 0;	
		return orgAfterSetupTextureStages();
	}

	static int (*orgSfxEnable)(void* a1);
	int SFX_Enable_Force(void* a1)
	{
#ifndef NDEBUG
		if (GetAsyncKeyState(VK_F3) & 0x8000)
		{
			return 0;
		}
#endif
		if (alwaysDrawSky)
		{
			return 1;
		}
		return orgSfxEnable(a1);
	}
}

namespace Timers
{
	static int64_t GetQPC()
	{
		LARGE_INTEGER time;
		QueryPerformanceCounter(&time);
		return time.QuadPart;
	}

	static LARGE_INTEGER Frequency;
	uint64_t GetTimeInMS()
	{
		// Calculate time in MS but with overflow avoidance
		const int64_t curTime = GetQPC();
		const int64_t freq = Frequency.QuadPart;

		const int64_t whole = (curTime / freq) * 1000;
		const int64_t part = (curTime % freq) * 1000 / freq;
		return whole + part;
	}

	static LARGE_INTEGER StartQPC;
	void Reset()
	{
		QueryPerformanceCounter(&StartQPC);

		// To fix issues occuring with the first physics tick having a zero delta,
		// knock the starting time off by a second. Hacky, but affects only the first physics tick/frame.
		StartQPC.QuadPart -= Frequency.QuadPart;
	}

	void Setup()
	{
		QueryPerformanceFrequency(&Frequency);
		Reset();
	}

	static DWORD WINAPI timeGetTime_Reset()
	{
		Reset();
		return 0;
	}
	static const auto pTimeGetTime_Reset = &timeGetTime_Reset;

	static DWORD WINAPI timeGetTime_Update()
	{
		const int64_t curTime = GetQPC() - StartQPC.QuadPart;
		const int64_t freq = Frequency.QuadPart;

		const int64_t whole = (curTime / freq) * 1000;
		const int64_t part = (curTime % freq) * 1000 / freq;
		return static_cast<DWORD>(whole + part);
	}
	static const auto pTimeGetTime_Update = &timeGetTime_Update;

	static DWORD WINAPI timeGetTime_Precise()
	{
		return static_cast<DWORD>(GetTimeInMS());
	}

	static void ReplaceFunction(void** funcPtr)
	{
		DWORD dwProtect;

		VirtualProtect(funcPtr, sizeof(*funcPtr), PAGE_READWRITE, &dwProtect);
		*funcPtr = &timeGetTime_Precise;
		VirtualProtect(funcPtr, sizeof(*funcPtr), dwProtect, &dwProtect);
	}

	static bool RedirectImports()
	{
		const DWORD_PTR instance = reinterpret_cast<DWORD_PTR>(GetModuleHandle(nullptr));
		const PIMAGE_NT_HEADERS ntHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(instance + reinterpret_cast<PIMAGE_DOS_HEADER>(instance)->e_lfanew);

		// Find IAT
		PIMAGE_IMPORT_DESCRIPTOR pImports = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(instance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

		for ( ; pImports->Name != 0; pImports++ )
		{
			if ( _stricmp(reinterpret_cast<const char*>(instance + pImports->Name), "winmm.dll") == 0 )
			{
				if ( pImports->OriginalFirstThunk != 0 )
				{
					const PIMAGE_THUNK_DATA pThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(instance + pImports->OriginalFirstThunk);

					for ( ptrdiff_t j = 0; pThunk[j].u1.AddressOfData != 0; j++ )
					{
						if ( strcmp(reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(instance + pThunk[j].u1.AddressOfData)->Name, "timeGetTime") == 0 )
						{
							void** pAddress = reinterpret_cast<void**>(instance + pImports->FirstThunk) + j;
							ReplaceFunction(pAddress);
							return true;
						}
					}
				}
				else
				{
					void** pFunctions = reinterpret_cast<void**>(instance + pImports->FirstThunk);

					for ( ptrdiff_t j = 0; pFunctions[j] != nullptr; j++ )
					{
						if ( pFunctions[j] == &::timeGetTime )
						{
							ReplaceFunction(&pFunctions[j]);
							return true;
						}
					}
				}
			}
		}
		return false;
	}
}

namespace ResolutionsList
{
	// Easiest to do this via runtime patching...
	std::vector<std::pair<void*, size_t>> placesToPatch;

	std::vector<MenuResolutionEntry> displayModeStorage;
	static constexpr uint32_t RELOCATION_INITIAL_THRESHOLD = 128;

	uint32_t (*orgGetDisplayModeCount)(uint32_t adapter);
	uint32_t GetDisplayModeCount_RelocateArray(uint32_t adapter)
	{
		const uint32_t modeCount = orgGetDisplayModeCount(adapter);
		const uint32_t modeCapacity = std::max(RELOCATION_INITIAL_THRESHOLD, displayModeStorage.size());
		if (modeCount > modeCapacity)
		{
			if (placesToPatch.empty())
			{
				// Failsafe, limit to the max number of resolutions stock allocations can handle
				return RELOCATION_INITIAL_THRESHOLD;
			}

			displayModeStorage.resize(modeCount);
			std::byte* buf = reinterpret_cast<std::byte*>(displayModeStorage.data());

			// Patch pointers, ugly but saves a lot of effort
			auto Protect = ScopedUnprotect::UnprotectSectionOrFullModule( GetModuleHandle( nullptr ), ".text" );
			
			using namespace Memory;
			for (const auto& addr : placesToPatch)
			{
				Patch(addr.first, buf+addr.second);
			}
		}
		return modeCount;
	}
}

namespace FindClosestDisplayMode
{
	static int (*orgCreateD3DDevice)();
	static int FindClosestDisplayMode_CreateD3DDevice()
	{
		// If running fullscreen, we must be matching a specific display mode or else device fails to create
		if (gd3dPP->Windowed == FALSE)
		{
			IDirect3D9* d3d = *gpD3D;

			const D3DFORMAT format = gd3dPP->BackBufferFormat == D3DFMT_A8R8G8B8 ? D3DFMT_X8R8G8B8 : gd3dPP->BackBufferFormat;

			// For the lack of a better idea, find a display mode with the closest screen area - or an exact match if one exists
			std::map<uint64_t, std::pair<UINT, UINT>> displayModesByArea;
			const UINT NumModes = d3d->GetAdapterModeCount(gGraphicsConfig->m_adapter, format);			
			for (UINT i = 0; i < NumModes; i++)
			{
				D3DDISPLAYMODE displayMode;
				if (SUCCEEDED(d3d->EnumAdapterModes(gGraphicsConfig->m_adapter, format, i, &displayMode)))
				{
					// If it's an exact match, we don't need to do anything - bail out
					if (gd3dPP->BackBufferWidth == displayMode.Width && gd3dPP->BackBufferHeight == displayMode.Height)
					{
						return orgCreateD3DDevice();
					}

					// Use operator[] so we overwrite an already existing entry
					displayModesByArea[static_cast<uint64_t>(displayMode.Width) * displayMode.Height] = {displayMode.Width, displayMode.Height};
				}
			}

			// No exact match was found, so find the closest display mode
			auto it = displayModesByArea.lower_bound(static_cast<uint64_t>(gd3dPP->BackBufferWidth) * gd3dPP->BackBufferHeight);
			if (it != displayModesByArea.end())
			{
				gd3dPP->BackBufferWidth = it->second.first;
				gd3dPP->BackBufferHeight = it->second.second;
			}
			else
			{
				// No resolutions as big as this one, use the last one from the list
				auto topIt = displayModesByArea.rbegin();
				gd3dPP->BackBufferWidth = topIt->second.first;
				gd3dPP->BackBufferHeight = topIt->second.second;
			}
		}
		return orgCreateD3DDevice();
	}
}

namespace HalfPixel
{
	using namespace DirectX;

	static float OffsetTexel(float val)
	{
		return std::ceil(val) - 0.5f;
	}

	static XMVECTOR ComputePoint(float x, float y, const XMMATRIX& rot, const XMVECTOR& trans)
	{
		return XMVectorAdd(trans, XMVector2TransformNormal(XMVectorSet(x, y, 0.0f, 0.0f), rot));
	}

	static float ComputeConstantScale(const XMVECTOR& pos, const XMMATRIX& view, const XMFLOAT4X4& proj)
	{
		const XMVECTOR ppcam0 = XMVector4Transform(pos, view);
		const XMVECTOR ppcam1 = XMVectorAdd(ppcam0, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));

		const XMVECTOR column0 = XMVectorSet(proj.m[0][0], proj.m[1][0], proj.m[2][0], 0.0f);
		const XMVECTOR column3 = XMVectorSet(proj.m[0][3], proj.m[1][3], proj.m[2][3], 0.0f);

		const float l1 = 1.0f / (XMVectorGetX(XMVector3Dot(ppcam0, column3)) + proj.m[3][3]);
		const float c1 = (XMVectorGetX(XMVector3Dot(ppcam0, column0)) + proj.m[3][0]) * l1;
		const float l2 = 1.0f / (XMVectorGetX(XMVector3Dot(ppcam1, column3)) + proj.m[3][3]);
		const float c2 = (XMVectorGetX(XMVector3Dot(ppcam1, column0)) + proj.m[3][0]) * l2;
		return 1.0f / ((c2 - c1) * Viewport_GetCurrent()->m_width);
	}

	// This and the above functions have been adapted for the game from "Textured Lines In D3D" by Pierre Terdiman
	// https://www.flipcode.com/archives/Textured_Lines_In_D3D.shtml
	void ComputeScreenQuad(const XMMATRIX& inverseview, const XMMATRIX& view, const XMFLOAT4X4* proj, XMVECTOR* verts, const XMVECTOR& p0, const XMVECTOR& p1, float size)
	{
		// Compute delta in camera space
		const XMVECTOR Delta = XMVector3TransformNormal(p1-p0, view);

		// Compute size factors
		float SizeP0 = size;
		float SizeP1 = size;

		if(proj != nullptr)
		{
			// Compute scales so that screen-size is constant
			SizeP0 *= ComputeConstantScale(p0, view, *proj);
			SizeP1 *= ComputeConstantScale(p1, view, *proj);
		}
		else
		{
			SizeP0 /= 2.0f;
			SizeP1 /= 2.0f;
		}

		// Compute quad vertices
		const float Theta0 = atan2f(-XMVectorGetX(Delta), -XMVectorGetY(Delta));
		const float c0 = SizeP0 * std::cos(Theta0);
		const float s0 = SizeP0 * std::sin(Theta0);
		*verts++ = ComputePoint(c0, -s0, inverseview, p0);
		*verts++ = ComputePoint(-c0, s0, inverseview, p0);

		const float Theta1 = atan2f(XMVectorGetX(Delta), XMVectorGetY(Delta));
		const float c1 = SizeP1 * std::cos(Theta1);
		const float s1 = SizeP1 * std::sin(Theta1);
		*verts++ = ComputePoint(-c1, s1, inverseview, p1);
		*verts++ = ComputePoint(c1, -s1, inverseview, p1);
	}

	void (*Core_Blitter2D_Tri2D_G)(BlitTri2D_G* tris, uint32_t numTris);
	void (*Core_Blitter3D_Tri3D_G)(BlitTri3D_G* tris, uint32_t numTris);

	static void** dword_936C0C;

	static void* Core_Blitter2D_Rect2D_G_JumpBack;
	__declspec(naked) void Core_Blitter2D_Rect2D_G_Original(BlitRect2D_G*, uint32_t)
	{
		__asm
		{
			sub		esp, 028h
			mov		eax, dword ptr [dword_936C0C]
			mov		eax, dword ptr [eax]
			jmp		[Core_Blitter2D_Rect2D_G_JumpBack]
		}
	}

	void Core_Blitter2D_Rect2D_G_HalfPixel(BlitRect2D_G* verts, uint32_t numVerts)
	{
		for (uint32_t i = 0; i < numVerts; i++)
		{
			for (float& val : verts[i].X)
			{
				val = OffsetTexel(val);
			}
			for (float& val : verts[i].Y)
			{
				val = OffsetTexel(val);
			}
		}
		(*gpd3dDevice)->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, FALSE);
		Core_Blitter2D_Rect2D_G_Original(verts, numVerts);
		(*gpd3dDevice)->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);
	}

	static void* Core_Blitter2D_Rect2D_GT_JumpBack;
	__declspec(naked) void Core_Blitter2D_Rect2D_GT_Original(BlitRect2D_GT*, uint32_t)
	{
		__asm
		{
			sub		esp, 028h
			mov		eax, dword ptr [dword_936C0C]
			mov		eax, dword ptr [eax]
			jmp		[Core_Blitter2D_Rect2D_GT_JumpBack]
		}
	}

	void Core_Blitter2D_Rect2D_GT_HalfPixel(BlitRect2D_GT* verts, uint32_t numRectangles)
	{
		for (uint32_t i = 0; i < numRectangles; i++)
		{
			for (float& val : verts[i].X)
			{
				val = OffsetTexel(val);
			}
			for (float& val : verts[i].Y)
			{
				val = OffsetTexel(val);
			}
		}
		(*gpd3dDevice)->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, FALSE);
		Core_Blitter2D_Rect2D_GT_Original(verts, numRectangles);
		(*gpd3dDevice)->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);
	}

	static void* Core_Blitter2D_Quad2D_G_JumpBack;
	__declspec(naked) void Core_Blitter2D_Quad2D_G_Original(BlitQuad2D_G*, uint32_t)
	{
		__asm
		{
			sub		esp, 028h
			mov		eax, dword ptr [dword_936C0C]
			mov		eax, dword ptr [eax]
			jmp		[Core_Blitter2D_Quad2D_G_JumpBack]
		}
	}

	void Core_Blitter2D_Quad2D_G_HalfPixel(BlitQuad2D_G* quads, uint32_t numQuads)
	{
		for (uint32_t i = 0; i < numQuads; i++)
		{
			for (float& val : quads[i].X)
			{
				val = OffsetTexel(val);
			}
			for (float& val : quads[i].Y)
			{
				val = OffsetTexel(val);
			}
		}
		Core_Blitter2D_Quad2D_G_Original(quads, numQuads);
	}

	static void* Core_Blitter2D_Quad2D_GT_JumpBack;
	__declspec(naked) void Core_Blitter2D_Quad2D_GT_Original(BlitQuad2D_GT*, uint32_t)
	{
		__asm
		{
			sub		esp, 028h
			mov		eax, dword ptr [dword_936C0C]
			mov		eax, dword ptr [eax]
			jmp		[Core_Blitter2D_Quad2D_GT_JumpBack]
		}
	}

	void Core_Blitter2D_Quad2D_GT_HalfPixel(BlitQuad2D_GT* quads, uint32_t numQuads)
	{
		for (uint32_t i = 0; i < numQuads; i++)
		{
			for (float& val : quads[i].X)
			{
				val = OffsetTexel(val);
			}
			for (float& val : quads[i].Y)
			{
				val = OffsetTexel(val);
			}
		}
		Core_Blitter2D_Quad2D_GT_Original(quads, numQuads);
	}

	static D3DMATRIX* pViewMatrix;
	static D3DMATRIX* pProjectionMatrix;

	static void* Core_Blitter2D_Line2D_G_JumpBack;
	__declspec(naked) void Core_Blitter2D_Line2D_G_Original(BlitLine2D_G*, uint32_t)
	{
		__asm
		{
			sub		esp, 010h
			mov		ecx, dword ptr [dword_936C0C]
			mov		ecx, dword ptr [ecx]
			jmp		[Core_Blitter2D_Line2D_G_JumpBack]
		}	
	}

	void Core_Blitter2D_Line2D_G_HalfPixelAndThickness(BlitLine2D_G* lines, uint32_t numLines)
	{
#ifndef NDEBUG
		if (GetAsyncKeyState(VK_F1) & 0x8000)
		{
			Core_Blitter2D_Line2D_G_Original(lines, numLines);
			return;
		}
#endif

		auto buf = _malloca(sizeof(BlitTri2D_G) * 2 * numLines);
		if (buf != nullptr)
		{
			using namespace DirectX;

			const XMMATRIX identityMatrix = XMMatrixIdentity();
			const float targetThickness = Viewport_GetCurrent()->m_height / 480.0f;

			BlitTri2D_G* tris = reinterpret_cast<BlitTri2D_G*>(buf);
			BlitTri2D_G* currentTri = tris;
			for (uint32_t i = 0; i < numLines; i++)
			{
				XMVECTOR quad[4];
				ComputeScreenQuad(identityMatrix, identityMatrix, nullptr, quad, XMVectorSet(std::floor(lines[i].X[0]), std::floor(lines[i].Y[0]), 0.0f, 1.0f),
													XMVectorSet(std::floor(lines[i].X[1]), std::floor(lines[i].Y[1]), 0.0f, 1.0f), targetThickness);

				// Make triangles from quad
				currentTri->X[0] = OffsetTexel(XMVectorGetX(quad[1]));
				currentTri->Y[0] = OffsetTexel(XMVectorGetY(quad[1]));
				currentTri->X[1] = OffsetTexel(XMVectorGetX(quad[0]));
				currentTri->Y[1] = OffsetTexel(XMVectorGetY(quad[0]));
				currentTri->X[2] = OffsetTexel(XMVectorGetX(quad[2]));
				currentTri->Y[2] = OffsetTexel(XMVectorGetY(quad[2]));
				currentTri->Z = lines[i].Z[0];
				std::fill(std::begin(currentTri->color), std::end(currentTri->color), lines[i].color[0]);
				currentTri++;

				currentTri->X[0] = OffsetTexel(XMVectorGetX(quad[2]));
				currentTri->Y[0] = OffsetTexel(XMVectorGetY(quad[2]));
				currentTri->X[1] = OffsetTexel(XMVectorGetX(quad[3]));
				currentTri->Y[1] = OffsetTexel(XMVectorGetY(quad[3]));
				currentTri->X[2] = OffsetTexel(XMVectorGetX(quad[1]));
				currentTri->Y[2] = OffsetTexel(XMVectorGetY(quad[1]));
				currentTri->Z = lines[i].Z[0];
				std::fill(std::begin(currentTri->color), std::end(currentTri->color), lines[i].color[1]);
				currentTri++;
			}

			Core_Blitter2D_Tri2D_G(tris, 2 * numLines);
		}
		_freea(buf);
	}

	static void* Core_Blitter3D_Line3D_G_JumpBack;
	__declspec(naked) void Core_Blitter3D_Line3D_G_Original(BlitLine3D_G*, uint32_t)
	{
		__asm
		{
			push	ebp
			mov		ebp, esp
			sub		esp, 8
			jmp		[Core_Blitter3D_Line3D_G_JumpBack]
		}
	}

	void Core_Blitter3D_Line3D_G_LineThickness(BlitLine3D_G* lines, uint32_t numLines)
	{
#ifndef NDEBUG
		if (GetAsyncKeyState(VK_F1) & 0x8000)
		{
			Core_Blitter3D_Line3D_G_Original(lines, numLines);
			return;
		}
#endif

		auto buf = _malloca(sizeof(BlitTri3D_G) * 2 * numLines);
		if (buf != nullptr)
		{
			const XMMATRIX viewMatrix = XMLoadFloat4x4(reinterpret_cast<XMFLOAT4X4*>(pViewMatrix));
			const XMMATRIX invViewMatrix = XMMatrixInverse(nullptr, viewMatrix);

			const float targetThickness = Graphics_GetScreenHeight() / 480.0f;

			BlitTri3D_G* tris = reinterpret_cast<BlitTri3D_G*>(buf);
			BlitTri3D_G* currentTri = tris;
			for (uint32_t i = 0; i < numLines; i++)
			{
				XMVECTOR quad[4];
				ComputeScreenQuad(invViewMatrix, viewMatrix, reinterpret_cast<const XMFLOAT4X4*>(pProjectionMatrix), quad,
					XMVectorSet(lines[i].X1, lines[i].Y1, lines[i].Z1, 1.0f), XMVectorSet(lines[i].X2, lines[i].Y2, lines[i].Z2, 1.0f), targetThickness);

				// Make triangles from quad
				XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(&currentTri->X1), quad[1]);
				XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(&currentTri->X2), quad[0]);
				XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(&currentTri->X3), quad[2]);
				std::fill(std::begin(currentTri->color), std::end(currentTri->color), lines[i].color[0]);
				currentTri++;

				XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(&currentTri->X1), quad[2]);
				XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(&currentTri->X2), quad[3]);
				XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(&currentTri->X3), quad[1]);
				std::fill(std::begin(currentTri->color), std::end(currentTri->color), lines[i].color[1]);
				currentTri++;
			}

			Core_Blitter3D_Tri3D_G(tris, 2 * numLines);
		}
		_freea(buf);
	}
}

namespace ScissorTacho
{
	__declspec(naked) int __cdecl ftol_fake()
	{
		__asm
		{
			sub		esp, 4
			fstp	dword ptr [esp]
			pop		eax
			retn
		}
	}

	static void (*orgHandyFunction_BlitTexture)(void* texture, int u1, int v1, int u2, int v2, int posX, int posY, int width, int height, uint32_t color);
	void HandyFunction_BlitTexture_Scissor(void* texture, int u1, int v1, float u2, int v2, int posX, int posY, float width, int height, uint32_t color)
	{
		if (width <= 0.0f)
		{
			return;
		}

		IDirect3DDevice9* device = *gpd3dDevice;

		if ((gD3DCaps->RasterCaps & D3DPRASTERCAPS_SCISSORTEST) != 0)
		{
			const float ScaleX = Graphics_GetScreenWidth() / GetScaledResolutionWidth();

			device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);

			const int viewportOffset = Viewport_GetCurrent()->m_left;
			const RECT scissorRect { static_cast<LONG>(posX * ScaleX) + viewportOffset, 0,
				static_cast<LONG>((posX+width) * ScaleX) + viewportOffset, static_cast<LONG>(Graphics_GetScreenHeight()) };
			device->SetScissorRect(&scissorRect);

			orgHandyFunction_BlitTexture(texture, u1, v1, 128, v2, posX, posY, (*gpPositionInfoMulti)->m_TachoWidth, height, color);

			device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		}
		else
		{
			orgHandyFunction_BlitTexture(texture, u1, v1, static_cast<int>(u2), v2, posX, posY, static_cast<int>(width), height, color);
		}
	}
}

namespace BetterBoxDrawing
{
	void DisplaySelectionBox(int posX, int posY, int width, int height, unsigned int alpha, int fill)
	{
		static constexpr uint32_t BOX_COLOR = 0xDCDCDC;

		const float Scale = GetScaledResolutionWidth();
		const float ScaleX = Graphics_GetScreenWidth() / Scale;
		const float ScaleY = Graphics_GetScreenHeight() / 480.0f;

		// The original game used lines, but we use rects for nice and tidy line thickness
		const float LineThickness = std::max(1.0f, std::floor(ScaleX));
		const float HalfLineThickness = LineThickness / 2.0f;

		BlitRect2D_G rects[5];
		{
			const uint32_t color = (alpha << 24) | BOX_COLOR;
			for (auto it = rects; it != rects+4; ++it)
			{
				std::fill(std::begin(it->color), std::end(it->color), color);
				it->Z = 0.0f;
			}
		}

		uint32_t rectsToDraw = 0;
		{
			BlitRect2D_G& top = rects[rectsToDraw++];
			top.X[0] = (posX * ScaleX) - HalfLineThickness;
			top.X[1] = ((posX + width) * ScaleX) + HalfLineThickness;
			top.Y[0] = (posY * ScaleY) - HalfLineThickness;
			top.Y[1] = (posY * ScaleY) + HalfLineThickness;
		}
		{
			BlitRect2D_G& right = rects[rectsToDraw++];
			right.X[0] = ((posX + width) * ScaleX) - HalfLineThickness;
			right.X[1] = ((posX + width) * ScaleX) + HalfLineThickness;
			right.Y[0] = (posY * ScaleY) + HalfLineThickness;
			right.Y[1] = ((posY + height) * ScaleY) - HalfLineThickness;
		}
		{
			BlitRect2D_G& bottom = rects[rectsToDraw++];
			bottom.X[0] = (posX * ScaleX) - HalfLineThickness;
			bottom.X[1] = ((posX + width) * ScaleX) + HalfLineThickness;
			bottom.Y[0] = ((posY + height) * ScaleY) - HalfLineThickness;
			bottom.Y[1] = ((posY + height) * ScaleY) + HalfLineThickness;
		}
		{
			BlitRect2D_G& left = rects[rectsToDraw++];
			left.X[0] = (posX * ScaleX) - HalfLineThickness;
			left.X[1] = (posX * ScaleX) + HalfLineThickness;
			left.Y[0] = (posY * ScaleY) + HalfLineThickness;
			left.Y[1] = ((posY + height) * ScaleY) - HalfLineThickness;
		}

		if (fill != 0)
		{
			const uint32_t color = ((alpha / 2) << 24) | BOX_COLOR;

			BlitRect2D_G& rect = rects[rectsToDraw++];
			rect.Z = 0.0f;
			std::fill(std::begin(rect.color), std::end(rect.color), color);

			rect.X[0] = (posX * ScaleX) + HalfLineThickness;
			rect.X[1] = ((posX + width) * ScaleX) - HalfLineThickness;

			rect.Y[0] = (posY * ScaleY) + HalfLineThickness;
			rect.Y[1] = ((posY + height) * ScaleY) - HalfLineThickness;
		}

		Core_Blitter2D_Rect2D_G(rects, rectsToDraw);
	}

	void Keyboard_DrawTextEntryBox(int posX, int posY, int width, int height, unsigned int color, int fill)
	{
		const float Scale = GetScaledResolutionWidth();
		const float ScaleX = Graphics_GetScreenWidth() / Scale;
		const float ScaleY = Graphics_GetScreenHeight() / 480.0f;

		// The original game used lines, but we use rects for nice and tidy line thickness
		const float LineThickness = std::max(1.0f, std::floor(ScaleX));
		const float HalfLineThickness = LineThickness / 2.0f;

		if (fill != 0)
		{
			BlitRect2D_G rect;
			rect.Z = 0.0f;
			std::fill(std::begin(rect.color), std::end(rect.color), color);

			rect.X[0] = (posX * ScaleX) - HalfLineThickness;
			rect.X[1] = ((posX + width) * ScaleX) + HalfLineThickness;
			
			rect.Y[0] = (posY * ScaleY) - HalfLineThickness;
			rect.Y[1] = ((posY + height) * ScaleY) + HalfLineThickness;
			Core_Blitter2D_Rect2D_G(&rect, 1);
			return;
		}

		BlitRect2D_G rects[4];
		for (BlitRect2D_G& rect : rects)
		{
			std::fill(std::begin(rect.color), std::end(rect.color), color);
			rect.Z = 0.0f;
		}

		{
			BlitRect2D_G& top = rects[0];
			top.X[0] = (posX * ScaleX) - HalfLineThickness;
			top.X[1] = ((posX + width) * ScaleX) + HalfLineThickness;
			top.Y[0] = (posY * ScaleY) - HalfLineThickness;
			top.Y[1] = (posY * ScaleY) + HalfLineThickness;
		}
		{
			BlitRect2D_G& right = rects[1];
			right.X[0] = ((posX + width) * ScaleX) - HalfLineThickness;
			right.X[1] = ((posX + width) * ScaleX) + HalfLineThickness;
			right.Y[0] = (posY * ScaleY) + HalfLineThickness;
			right.Y[1] = ((posY + height) * ScaleY) - HalfLineThickness;
		}
		{
			BlitRect2D_G& bottom = rects[2];
			bottom.X[0] = (posX * ScaleX) - HalfLineThickness;
			bottom.X[1] = ((posX + width) * ScaleX) + HalfLineThickness;
			bottom.Y[0] = ((posY + height) * ScaleY) - HalfLineThickness;
			bottom.Y[1] = ((posY + height) * ScaleY) + HalfLineThickness;
		}
		{
			BlitRect2D_G& left = rects[3];
			left.X[0] = (posX * ScaleX) - HalfLineThickness;
			left.X[1] = (posX * ScaleX) + HalfLineThickness;
			left.Y[0] = (posY * ScaleY) + HalfLineThickness;
			left.Y[1] = ((posY + height) * ScaleY) - HalfLineThickness;
		}

		Core_Blitter2D_Rect2D_G(rects, std::size(rects));
	}

	void HandyFunction_DrawClipped2DBox(int posX, int posY, int width, int height, unsigned int color, int clipX, int clipY, int clipWidth, int clipHeight, int fill)
	{
		const float Scale = GetScaledResolutionWidth();
		const float ScaleX = Graphics_GetScreenWidth() / Scale;
		const float ScaleY = Graphics_GetScreenHeight() / 480.0f;

		// Cut height/width down by 1px to make clipped boxes consistent with clipped fonts (original bug most likely)
		const float ClipX1 = clipX * ScaleX, ClipX2 = (clipX + clipWidth) * ScaleX - 1.0f;
		const float ClipY1 = clipY * ScaleY, ClipY2 = (clipY + clipHeight) * ScaleY - 1.0f;

		// The original game used lines, but we use rects for nice and tidy line thickness
		const float LineThickness = std::max(1.0f, std::floor(ScaleX));
		const float HalfLineThickness = LineThickness / 2.0f;

		if (fill != 0)
		{
			BlitRect2D_G rect;
			rect.Z = 0.0f;
			std::fill(std::begin(rect.color), std::end(rect.color), color);

			rect.X[0] = (posX * ScaleX) - HalfLineThickness;
			rect.X[1] = ((posX + width) * ScaleX) + HalfLineThickness;

			rect.Y[0] = (posY * ScaleY) - HalfLineThickness;
			rect.Y[1] = ((posY + height) * ScaleY) + HalfLineThickness;
			if (HandyFunction_Clip2DRect(&rect, ClipX1, ClipY1, ClipX2, ClipY2) != 0)
			{
				Core_Blitter2D_Rect2D_G(&rect, 1);
			}
			return;
		}

		BlitRect2D_G rects[4];
		for (BlitRect2D_G& rect : rects)
		{
			std::fill(std::begin(rect.color), std::end(rect.color), color);
			rect.Z = 0.0f;
		}

		uint32_t numSides = 0;
		{
			BlitRect2D_G& top = rects[numSides];
			top.X[0] = (posX * ScaleX) - HalfLineThickness;
			top.X[1] = ((posX + width) * ScaleX) + HalfLineThickness;
			top.Y[0] = (posY * ScaleY) - HalfLineThickness;
			top.Y[1] = (posY * ScaleY) + HalfLineThickness;
			if (HandyFunction_Clip2DRect(&top, ClipX1, ClipY1, ClipX2, ClipY2) != 0)
			{
				numSides++;
			}
		}
		{
			BlitRect2D_G& right = rects[numSides];
			right.X[0] = ((posX + width) * ScaleX) - HalfLineThickness;
			right.X[1] = ((posX + width) * ScaleX) + HalfLineThickness;
			right.Y[0] = (posY * ScaleY) + HalfLineThickness;
			right.Y[1] = ((posY + height) * ScaleY) - HalfLineThickness;
			if (HandyFunction_Clip2DRect(&right, ClipX1, ClipY1, ClipX2, ClipY2) != 0)
			{
				numSides++;
			}
		}
		{
			BlitRect2D_G& bottom = rects[numSides];
			bottom.X[0] = (posX * ScaleX) - HalfLineThickness;
			bottom.X[1] = ((posX + width) * ScaleX) + HalfLineThickness;
			bottom.Y[0] = ((posY + height) * ScaleY) - HalfLineThickness;
			bottom.Y[1] = ((posY + height) * ScaleY) + HalfLineThickness;
			if (HandyFunction_Clip2DRect(&bottom, ClipX1, ClipY1, ClipX2, ClipY2) != 0)
			{
				numSides++;
			}
		}
		{
			BlitRect2D_G& left = rects[numSides];
			left.X[0] = (posX * ScaleX) - HalfLineThickness;
			left.X[1] = (posX * ScaleX) + HalfLineThickness;
			left.Y[0] = (posY * ScaleY) + HalfLineThickness;
			left.Y[1] = ((posY + height) * ScaleY) - HalfLineThickness;
			if (HandyFunction_Clip2DRect(&left, ClipX1, ClipY1, ClipX2, ClipY2) != 0)
			{
				numSides++;
			}
		}

		Core_Blitter2D_Rect2D_G(rects, numSides);
	}

	void DrawCountdownOutline(float X1, float Y1, float X2, float Y2, unsigned int color)
	{
		const float Scale = GetScaledResolutionWidth();
		const float ScaleX = Graphics_GetScreenWidth() / Scale;

		// The original game used lines, but we use rects for nice and tidy line thickness
		const float LineThickness = std::max(1.0f, std::floor(ScaleX));
		const float HalfLineThickness = LineThickness / 2.0f;

		BlitRect2D_G rects[4];
		for (BlitRect2D_G& rect : rects)
		{
			std::fill(std::begin(rect.color), std::end(rect.color), color);
			rect.Z = 0.0f;
		}

		{
			BlitRect2D_G& top = rects[0];
			top.X[0] = X1 - HalfLineThickness;
			top.X[1] = X2 + HalfLineThickness;
			top.Y[0] = Y1 - HalfLineThickness;
			top.Y[1] = Y1 + HalfLineThickness;
		}
		{
			BlitRect2D_G& right = rects[1];
			right.X[0] = X2 - HalfLineThickness;
			right.X[1] = X2 + HalfLineThickness;
			right.Y[0] = Y1 + HalfLineThickness;
			right.Y[1] = Y2 - HalfLineThickness;
		}
		{
			BlitRect2D_G& bottom = rects[2];
			bottom.X[0] = X1 - HalfLineThickness;
			bottom.X[1] = X2 + HalfLineThickness;
			bottom.Y[0] = Y2 - HalfLineThickness;
			bottom.Y[1] = Y2 + HalfLineThickness;
		}
		{
			BlitRect2D_G& left = rects[3];
			left.X[0] = X1 - HalfLineThickness;
			left.X[1] = X1 + HalfLineThickness;
			left.Y[0] = Y1 + HalfLineThickness;
			left.Y[1] = Y2 - HalfLineThickness;
		}

		Core_Blitter2D_Rect2D_G(rects, std::size(rects));
	}

	static thread_local float SavedPosY;
	void Blitter2D_Line2D_G_FirstLine(BlitLine2D_G* lines, uint32_t /*numLines*/)
	{
		SavedPosY = lines->Y[0];
	}

	void Blitter2D_Line2D_G_SecondLine(BlitLine2D_G* lines, uint32_t /*numLines*/)
	{
		DrawCountdownOutline(lines->X[0], SavedPosY, lines->X[1], lines->Y[1], lines->color[0]);
	}

	void Blitter2D_Line2D_G_NOP(BlitLine2D_G* /*lines*/, uint32_t /*numLines*/)
	{
	
	}
}

// Hack to scretch full-width solid backgrounds to the entire screen, saves 30+ patches
namespace SolidRectangleWidthHack
{
	static void* HandyFunction_Draw2DBox_JumpBack;
	__declspec(naked) void HandyFunction_Draw2DBox_Original(int /*posX*/, int /*posY*/, int /*width*/, int /*height*/, int /*color*/)
	{
		__asm
		{
			sub		esp, 50h
			push	esi
			push	edi
			jmp		[HandyFunction_Draw2DBox_JumpBack]
		}
	}

	void HandyFunction_Draw2DBox_Hack(int posX, int posY, int width, int height, int color)
	{
		// If it's a draw spanning the entire width, stretch it automatically.
		if (posX == 0 && width == 640)
		{
			width = static_cast<int>(std::ceil(GetScaledResolutionWidth()));
		}
		HandyFunction_Draw2DBox_Original(posX, posY, width, height, color);
	}

	static void* CMR3Font_SetViewport_JumpBack;
	__declspec(naked) void CMR3Font_SetViewport_Original(int /*a1*/, int /*x1*/, int /*y1*/, int /*x2*/, int /*y2*/)
	{
		__asm
		{
			mov		eax, [esp+4]
			push	ebx
			jmp		[CMR3Font_SetViewport_JumpBack]
		}
	}

	void CMR3Font_SetViewport_Hack(int a1, int x1, int y1, int x2, int y2)
	{
		// If it's a viewport spanning the entire width, stretch it automatically.
		if (x1 == 0 && x2 == 640)
		{
			x2 = static_cast<int>(std::ceil(GetScaledResolutionWidth()));
		}
		CMR3Font_SetViewport_Original(a1, x1, y1, x2, y2);
	}
}

namespace ConstantViewports
{
	template<std::size_t Index>
	void (*orgViewport_SetDimension)(D3DViewport* viewport, int left, int top, int right, int bottom);

	template<std::size_t Index>
	static void Viewport_SetDimension(D3DViewport* viewport, int left, int top, int right, int bottom)
	{
		orgViewport_SetDimension<Index>(viewport, left, top, right, bottom);
		Viewport_SetAspectRatio(viewport, 1.0f, 1.0f);
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), orgViewport_SetDimension<Ctr << 16 | I>, Viewport_SetDimension<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}
}

void CMR3Font_BlitText_CalibrateAxisName(uint8_t a1, const char* text, int16_t /*posX*/, int16_t posY, uint32_t color, int /*align*/)
{
	CMR3Font_BlitText_Center(a1, text, 140, posY + 10, color, 0x14);
}

void CMR3Font_BlitText_RetiredFromRace(uint8_t a1, const char* text, int16_t posX, int16_t posY, uint32_t color, int align)
{
	const int32_t scaledPosX = posX * static_cast<int32_t>(GetScaledResolutionWidth());
	const int32_t scaledPosY = posY * 480;
	CMR3Font_BlitText(a1, text, static_cast<int16_t>(scaledPosX / Graphics_GetScreenWidth()), static_cast<int16_t>(scaledPosY / Graphics_GetScreenHeight()), color, align);
}

void CMR3Font_BlitText_SecretsScreenNoAutoSave(uint8_t a1, const char* text, int16_t posX, int16_t posY, uint32_t color, int align)
{
	if (posY == 186)
	{
		CMR3Font_BlitText_Center(a1, text, posX, posY, color, align);
	}
	else
	{
		CMR3Font_BlitText(a1, text, posX, posY, color, align);
	}
}

namespace ScaledTexturesSupport
{
	D3DTexture* CreateTexture_Misc_Scaled_Internal(D3DTexture* result, const char* name)
	{
		if (result != nullptr && name != nullptr)
		{
			struct TexData
			{
				uint32_t width, height;
				bool useNearest = false;
			};

			static const std::map<std::string_view, TexData, std::less<>> textureDimensions = {
				{ "Arrow1Player", { 32, 16 } },
				{ "ArrowMultiPlayer", { 32, 16 } },
				{ "ArrowSmall", { 16, 16 } },
				{ "Base", { 128, 128 } },
				{ "certina", { 64, 8 } },
				{ "Colour", { 128, 128, true } },
				{ "Ck_base", { 128, 64 } },
				{ "Ck_00", { 32, 32 } },
				{ "Ck_01", { 32, 32 } },
				{ "Ck_02", { 32, 32 } },
				{ "Ck_03", { 32, 32 } },
				{ "Ck_04", { 32, 32 } },
				{ "Ck_05", { 32, 32 } },
				{ "colin3_2", { 256, 64 } },
				{ "ct_3", { 64, 64, true } },
				{ "dialcntr", { 32, 32 } },
				{ "infobox", { 128, 128 } },
				{ "MiniStageBanner", { 128, 32 } },
				{ "osd_glow", { 32, 32 } },
				{ "rescert", { 128, 32 } },
				{ "swiss", { 128, 8 } },
				{ "AUS", { 32, 16 } },
				{ "FIN", { 32, 16 } },
				{ "GRE", { 32, 16 } },
				{ "JAP", { 32, 16 } },
				{ "SPA", { 32, 16 } },
				{ "SWE", { 32, 16 } },
				{ "UK", { 32, 16 } },
				{ "USA", { 32, 16 } },
			};
			auto it = textureDimensions.find(name);
			if (it != textureDimensions.end())
			{
				result->m_width = it->second.width;
				result->m_height = it->second.height;

				if (it->second.useNearest)
				{
					Core_Texture_SetFilteringMethod(result, D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_POINT);
				}
			}
		}
		return result;
	}

	template<std::size_t Index>
	D3DTexture* (*orgCreateTexture_Misc_Scaled)(void* a1, const char* name, int a3, int a4, int a5);

	template<std::size_t Index>
	D3DTexture* CreateTexture_Misc_Scaled(void* a1, const char* name, int a3, int a4, int a5)
	{
		D3DTexture* result = orgCreateTexture_Misc_Scaled<Index>(a1, name, a3, a4, a5);
		return CreateTexture_Misc_Scaled_Internal(result, name);
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl_Misc_Scaled(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), orgCreateTexture_Misc_Scaled<Ctr << 16 | I>, CreateTexture_Misc_Scaled<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach_Misc_Scaled(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl_Misc_Scaled<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}

	static uint32_t nextFontScale = 1;
	static bool nextFontNearest = false;

	static char* (*PlatformiseTextureFilename)(char* path);
	static char* PlatformiseTextureFilename_GetScale(char* path)
	{
		char* result = PlatformiseTextureFilename(path);

		// With merged locales, this path point at a subdirectory, so be mindful
		const std::filesystem::path stdPath(result);
		const std::filesystem::path iniPath = GetPathToGameDir() / stdPath.parent_path() / L"fonts.ini";
		const std::wstring fontName = stdPath.stem();

		nextFontScale = GetPrivateProfileIntW(fontName.c_str(), L"Scale", 1, iniPath.c_str());
		UINT useNearestFilter = GetPrivateProfileIntW(fontName.c_str(), L"NearestFilter", -1, iniPath.c_str());
		if (useNearestFilter == -1)
		{
			// Use hardcoded defaults if there is no INI entry for this font
			static const std::wstring_view fontsWithNearest[] = { L"kro_20", L"gears", L"time", L"speed" };
			nextFontNearest = std::find(std::begin(fontsWithNearest), std::end(fontsWithNearest), fontName) != std::end(fontsWithNearest);
		}
		else
		{
			nextFontNearest = useNearestFilter != 0;
		}

		return result;
	}

	static D3DTexture* (*orgCreateTexture_Font)(void* a1, const char* name, int a3, int a4, int a5);
	D3DTexture* CreateTexture_Font_Scaled(void* a1, const char* name, int a3, int a4, int a5)
	{
		D3DTexture* result = orgCreateTexture_Font(a1, name, a3, a4, a5);
		if (result != nullptr)
		{
			const uint32_t scale = std::exchange(nextFontScale, 1);
			result->m_width /= scale;
			result->m_height /= scale;

			if (std::exchange(nextFontNearest, false))
			{
				Core_Texture_SetFilteringMethod(result, D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_POINT);
			}
			else
			{
				Core_Texture_SetFilteringMethod(result, D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_LINEAR);
			}
		}
		return result;
	}

	void Texture_SetFilteringMethod_NOP(D3DTexture*, void*, void*, void*)
	{
	}
}

namespace NewAdvancedGraphicsOptions
{
	std::map<std::tuple<uint32_t, uint32_t, int32_t>, std::vector<uint32_t>> refreshRatesMap; // Width/height/bit depth

	int32_t GetBitDepth(D3DFORMAT format)
	{
		switch (format)
		{
		case D3DFMT_R8G8B8:
		case D3DFMT_A8R8G8B8:
		case D3DFMT_X8R8G8B8:
			return 1;
		case D3DFMT_R5G6B5:
		case D3DFMT_X1R5G5B5:
		case D3DFMT_A1R5G5B5:
			return 0;
		default:
			break;
		}

		return -1;
	}

	const std::vector<uint32_t>* GetRefreshRatesForMenuEntry(const MenuResolutionEntry* entry)
	{
		if (entry != nullptr)
		{
			auto it = refreshRatesMap.find({entry->m_width, entry->m_height, GetBitDepth(entry->m_format)});
			if (it != refreshRatesMap.end())
			{
				return &it->second;
			}
		}
		return nullptr;
	}

	static uint32_t (*orgGetNumModes)(uint32_t adapter);
	uint32_t GetNumModes_PopulateRefreshRates(uint32_t adapter)
	{
		const uint32_t numModes = orgGetNumModes(adapter);

		refreshRatesMap.clear();
		for (uint32_t i = 0; i < numModes; i++)
		{
			const Graphics_Mode* mode = Graphics_GetMode(adapter, i);

			const int32_t bitDepth = GetBitDepth(mode->m_format);
			if (mode->m_isValid == 0 || bitDepth < 0)
			{
				continue;
			}

			auto& rate = refreshRatesMap[{mode->m_width, mode->m_height, bitDepth}];
			rate.insert(std::lower_bound(rate.begin(), rate.end(), mode->m_refreshRate), mode->m_refreshRate);
		}

		return numModes;
	}


	BOOL WINAPI AdjustWindowRectEx_NOP(LPRECT /*lpRect*/, DWORD /*dwStyle*/, BOOL /*bMenu*/, DWORD /*dwExStyle*/)
	{
		return TRUE;
	}
	static const auto pAdjustWindowRectEx_NOP = &AdjustWindowRectEx_NOP;

	HWND WINAPI FindWindowExA_IgnoreWindowName(HWND hWndParent, HWND hWndChildAfter, LPCSTR lpszClass, LPCSTR /*lpszWindow*/)
	{
		return FindWindowExA(hWndParent, hWndChildAfter, lpszClass, nullptr);
	}
	static const auto pFindWindowExA_IgnoreWindowName = &FindWindowExA_IgnoreWindowName;

	HINSTANCE* ghInstance;
	HWND* ghWindow;
	void Main_WindowDestructor()
	{
		DestroyWindow(*ghWindow);
		*ghWindow = nullptr;
	}

	bool ResizingViaSetWindowPos = false;

	static LRESULT (CALLBACK *orgWndProc)(HWND, UINT, WPARAM, LPARAM);
	LRESULT CALLBACK CustomWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_SIZING: // Block the message from reaching the game
		case WM_SETCURSOR:
		{
			return DefWindowProcA(hwnd, uMsg, wParam, lParam);
		}
		case WM_WINDOWPOSCHANGED: // Prevent SetWindowPos from recursing into our WM_SIZE
		{
			if (ResizingViaSetWindowPos)
			{
				return 0;
			}
			break;
		}
		case WM_GETMINMAXINFO:
		{
			auto lpMMI = reinterpret_cast<LPMINMAXINFO>(lParam);
			lpMMI->ptMinTrackSize.x = 640;
			lpMMI->ptMinTrackSize.y = 480;
			return 0;
		}
		default:
			break;
		}
		return orgWndProc(hwnd, uMsg, wParam, lParam);
	}

	static void GetDesiredWindowStyle(uint32_t displayMode, DWORD& dwStyle, DWORD& dwExStyle)
	{
		dwStyle = WS_VISIBLE;
		dwExStyle = WS_EX_APPWINDOW;
		if (displayMode == 1) // Non-borderless windowed only
		{
			dwStyle |= WS_OVERLAPPEDWINDOW;
		}
		else
		{
			dwStyle |= WS_POPUP;
		}
	}

	void (*orgGraphics_Initialise)(Graphics_Config* config);
	void Graphics_Initialise_NewOptions(Graphics_Config* config)
	{
		using namespace Registry;

		const uint32_t displayMode = GetRegistryDword(REGISTRY_SECTION_NAME, DISPLAY_MODE_KEY_NAME).value_or(0);

		config->m_windowed = displayMode != 0;
		config->m_borderless = displayMode == 2;

		config->m_presentationInterval = GetRegistryDword(REGISTRY_SECTION_NAME, VSYNC_KEY_NAME).value_or(1) != 0
					? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

		config->m_refreshRate = GetRegistryDword(REGISTRY_SECTION_NAME, REFRESH_RATE_KEY_NAME).value_or(0);

		CMR_FE_SetAnisotropicLevel(GetRegistryDword(REGISTRY_SECTION_NAME, ANISOTROPIC_KEY_NAME).value_or(0));

		RenderState_InitialiseFallbackFilters(config->m_adapter);

		orgGraphics_Initialise(config);
	}

	BOOL CreateClassAndWindow(HINSTANCE hInstance, HWND* outWindow, LPCSTR lpClassName, LRESULT (CALLBACK *wndProc)(HWND, UINT, WPARAM, LPARAM), RECT rect)
	{
		ghWindow = outWindow;
		*ghInstance = hInstance;
		orgWndProc = wndProc;

		const int32_t Adapter = Registry::GetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"ADAPTER").value_or(0);
		const RECT& adapterRect = GetAdapterRect(Adapter);

		// Add a "missing" rect offset
		rect.left += adapterRect.left;
		rect.right += adapterRect.left;
		rect.top += adapterRect.top;
		rect.bottom += adapterRect.top;

		WNDCLASSEXA wndClass { sizeof(wndClass) };
		wndClass.lpfnWndProc = CustomWndProc;
		wndClass.hInstance = hInstance;
		wndClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
		wndClass.lpszClassName = lpClassName;
		wndClass.style = CS_VREDRAW|CS_HREDRAW;
		wndClass.hIcon = SmallIcon = LoadIcon(hInstance, MAKEINTRESOURCE(104));
		if (wndClass.hIcon == nullptr)
		{
			// Extract the icon from Rally_3PC.ico instead
			wil::unique_cotaskmem_string pathToExe;
			if (SUCCEEDED(wil::GetModuleFileNameW(hInstance, pathToExe)))
			{
				wndClass.hIcon = SmallIcon = ExtractIconW(hInstance, std::filesystem::path(pathToExe.get()).replace_filename(L"Rally_3PC.ico").c_str(), 0);
			}
		}
		if (RegisterClassExA(&wndClass) != 0)
		{
			using namespace Registry;

			const uint32_t displayMode = GetRegistryDword(REGISTRY_SECTION_NAME, DISPLAY_MODE_KEY_NAME).value_or(0);
			const char* windowName = "Colin McRae Rally 3";

			DWORD dwStyle = 0;
			DWORD dwExStyle = 0;
			GetDesiredWindowStyle(displayMode, dwStyle, dwExStyle);
			AdjustWindowRectEx(&rect, dwStyle, FALSE, dwExStyle);

			HWND window;
			if (displayMode == 1) // Non-borderless windowed only
			{
				window = CreateWindowExA(dwExStyle, lpClassName, windowName, dwStyle, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, hInstance, nullptr);
			}
			else
			{
				window = CreateWindowExA(dwExStyle, lpClassName, windowName, dwStyle, adapterRect.left, adapterRect.top,
					adapterRect.right - adapterRect.left, adapterRect.bottom - adapterRect.top, nullptr, nullptr, hInstance, nullptr);
			}
			*outWindow = window;
			if (window != nullptr)
			{
				// Spam a WM_TIMER message every 2 seconds just to keep the message pump alive, as else the game has a habit of entering a "hung" state
				SetTimer(window, reinterpret_cast<UINT_PTR>(&CreateClassAndWindow), 2000, nullptr);
				SetFocus(window);
				Destruct_AddDestructor(Destruct_GetCoreDestructorGroup(), Main_WindowDestructor);
				return TRUE;
			}
		}

		return FALSE;
	}

	LONG WINAPI SetWindowLongA_NOP(HWND /*hWnd*/, int /*nIndex*/, LONG /*dwNewLong*/)
	{
		return 0;
	}
	static const auto pSetWindowLongA_NOP = &SetWindowLongA_NOP;

	static int savedWidth, savedHeight;
	BOOL WINAPI SetWindowPos_Adjust(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
	{
		ResizingViaSetWindowPos = true;

		const Graphics_Config& config = Graphics_GetCurrentConfig();

		uint32_t displayMode = 0;
		if (config.m_windowed != 0 && config.m_borderless == 0) displayMode = 1;
		else if (config.m_windowed != 0 && config.m_borderless != 0) displayMode = 2;

		RECT rect { X, Y, X+cx, Y+cy };
		DWORD dwStyle = 0;
		DWORD dwExStyle = 0;
		GetDesiredWindowStyle(displayMode, dwStyle, dwExStyle);
		AdjustWindowRectEx(&rect, dwStyle, FALSE, dwExStyle);

		savedWidth = rect.right - rect.left;
		savedHeight = rect.bottom - rect.top;
		SetWindowLongPtr(hWnd, GWL_STYLE, dwStyle);
		BOOL result = SetWindowPos(hWnd, hWndInsertAfter, rect.left, rect.top, 0, 0, uFlags|SWP_FRAMECHANGED|SWP_NOSIZE); // Only style and move here, resize later

		SendMessage(hWnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(SmallIcon));

		ResizingViaSetWindowPos = false;
		return result;
	}
	static const auto pSetWindowPos_Adjust = &SetWindowPos_Adjust;

	void ResizeWindowAndUpdateConfig(Graphics_Config* config)
	{
		if (config->m_windowed != 0)
		{
			const RECT& desktopRect = GetAdapterRect(config->m_adapter);

			RECT& rect = config->m_windowRect;
			const LONG width = desktopRect.right - desktopRect.left;
			const LONG height = desktopRect.bottom - desktopRect.top;
			if (config->m_borderless == 0)
			{		
				rect.left = desktopRect.left + ((width - config->m_resWidth) / 2);
				rect.right = rect.left + config->m_resWidth;
				rect.top = desktopRect.top + ((height - config->m_resHeight) / 2);
				rect.bottom = rect.top + config->m_resHeight;
			}
			else
			{
				// When resizing to borderless, span the entire desktop
				rect = desktopRect;
			}

			// Force the game to resize the window
			gd3dPP->Windowed = static_cast<BOOL>(-1);
		}

		// Those are "missing" from the Change call
		gGraphicsConfig->m_borderless = config->m_borderless;
		gGraphicsConfig->m_presentationInterval = config->m_presentationInterval;

		RenderState_InitialiseFallbackFilters(config->m_adapter);
	}

	void AfterChangeResizeWindowAgain(const Graphics_Config* config)
	{
		if (config->m_windowed != 0)
		{
			// Resize the window again after device reset
			SetWindowPos(*ghWindow, nullptr, 0, 0, savedWidth, savedHeight, SWP_NOMOVE|SWP_NOZORDER|SWP_NOREDRAW|SWP_NOACTIVATE);
		}
	}

	template<std::size_t Index>
	uint32_t (*orgGraphics_Change)(Graphics_Config*);

	template<std::size_t Index>
	uint32_t Graphics_Change_ResizeWindow(Graphics_Config* config)
	{
		ResizeWindowAndUpdateConfig(config);
		const uint32_t result = orgGraphics_Change<Index>(config);
		AfterChangeResizeWindowAgain(config);
		return result;
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl_Graphics_Change(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), orgGraphics_Change<Ctr << 16 | I>, Graphics_Change_ResizeWindow<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach_Graphics_Change(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl_Graphics_Change<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}

	static void* PC_GraphicsAdvanced_Display_NewOptionsJumpBack;
	__declspec(naked) void PC_GraphicsAdvanced_Display_CaseNewOptions()
	{
		__asm
		{
			push	dword ptr [esp+328h-310h] // onColor
			push	edi // offColor
			push	eax // entryID
			push	dword ptr [esp+334h-314h] // posY
			push	dword ptr [esp+338h+8] // interp
			push	dword ptr [esp+33Ch+4] // menu
			call	PC_GraphicsAdvanced_Display_NewOptions
			jmp		[PC_GraphicsAdvanced_Display_NewOptionsJumpBack]
		}
	}

	static void* PC_GraphicsAdvanced_Handle_JumpBack;
	__declspec(naked) int PC_GraphicsAdvanced_Handle_Original(MenuDefinition* /*menu*/, uint32_t /*a2*/, int /*a3*/)
	{
		__asm
		{
			sub		esp, 50h
			lea		eax, [esp+50h-50h]
			jmp		[PC_GraphicsAdvanced_Handle_JumpBack]
		}
	}

	static int gnCurrentWindowMode;
	static int gnCurrentDisplayMode = -1;
	int PC_GraphicsAdvanced_Handle_NewOptions(MenuDefinition* menu, uint32_t a2, int a3)
	{
		const int adapter = menu->m_entries[EntryID::GRAPHICS_ADV_DRIVER].m_value;
		const int displayMode = menu->m_entries[EntryID::GRAPHICS_ADV_RESOLUTION].m_value;
		const Graphics_Config& config = Graphics_GetCurrentConfig();

		// Only care about switching to/from windowed mode, incl. borderless
		if (adapter == *gnCurrentAdapter && gnCurrentWindowMode != config.m_windowed)
		{
			PC_GraphicsAdvanced_PopulateFromCaps(menu, adapter, adapter);
			PC_GraphicsAdvanced_PopulateFromCaps_NewOptions(menu, adapter, adapter);
		}
		if (adapter != *gnCurrentAdapter && *gnCurrentAdapter != -1)
		{
			const int numDisplayModes = CMR_ValidateModeFormats(adapter);
			menu->m_entries[EntryID::GRAPHICS_ADV_RESOLUTION].m_entryDataInt = numDisplayModes;
			menu->m_entries[EntryID::GRAPHICS_ADV_RESOLUTION].m_value = std::max(1, numDisplayModes) - 1;
			gnCurrentDisplayMode = -2; // Force refresh rates to update
		}
		gnCurrentWindowMode = config.m_windowed;

		if (displayMode != gnCurrentDisplayMode)
		{
			if (gnCurrentDisplayMode != -1)
			{
				const int numRefreshRates = CMR_GetNumRefreshRates(GetMenuResolutionEntry(adapter, displayMode));
				menu->m_entries[EntryID::GRAPHICS_ADV_REFRESHRATE].m_entryDataInt = numRefreshRates;
				menu->m_entries[EntryID::GRAPHICS_ADV_REFRESHRATE].m_value = std::max(1, numRefreshRates) - 1;
			}
			gnCurrentDisplayMode = displayMode;
		}

		return PC_GraphicsAdvanced_Handle_Original(menu, a2, a3);
	}

	uint32_t (*orgGraphics_SetupRenderFromMenuOptions)(Graphics_Config*);
	uint32_t Graphics_Change_SetupRenderFromMenuOptions(Graphics_Config* config)
	{
		const uint32_t displayMode = gmoFrontEndMenus[MenuID::GRAPHICS_ADVANCED].m_entries[EntryID::GRAPHICS_ADV_DISPLAYMODE].m_value;
		config->m_windowed = displayMode != 0;
		config->m_borderless = displayMode == 2;

		config->m_presentationInterval = gmoFrontEndMenus[MenuID::GRAPHICS_ADVANCED].m_entries[EntryID::GRAPHICS_ADV_VSYNC].m_value != 0
			? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

		config->m_refreshRate = CMR_GetRefreshRateFromIndex(GetMenuResolutionEntry(
			gmoFrontEndMenus[MenuID::GRAPHICS_ADVANCED].m_entries[EntryID::GRAPHICS_ADV_DRIVER].m_value,
			gmoFrontEndMenus[MenuID::GRAPHICS_ADVANCED].m_entries[EntryID::GRAPHICS_ADV_RESOLUTION].m_value),
			gmoFrontEndMenus[MenuID::GRAPHICS_ADVANCED].m_entries[EntryID::GRAPHICS_ADV_REFRESHRATE].m_value);

		CMR_FE_SetAnisotropicLevel(gmoFrontEndMenus[MenuID::GRAPHICS_ADVANCED].m_entries[EntryID::GRAPHICS_ADV_ANISOTROPIC].m_value);

		return orgGraphics_SetupRenderFromMenuOptions(config);
	}

	static D3DCAPS9* (*orgGraphics_GetAdapterCaps)(D3DCAPS9* hdc, int index);
	D3DCAPS9* Graphics_GetAdapterCaps_DisableGammaForWindow(D3DCAPS9* hdc, int index)
	{
		D3DCAPS9* result = orgGraphics_GetAdapterCaps(hdc, index);
		const Graphics_Config& config = Graphics_GetCurrentConfig();
		if (config.m_windowed != 0)
		{
			result->Caps2 &= ~D3DCAPS2_FULLSCREENGAMMA;
		}
		return result;
	}


	// Anisotropic Filtering
	static void* RenderState_SetSamplerState_JumpBack;
	static int* guSSChanges;
	__declspec(naked) void RenderState_SetSamplerState_Original(DWORD /*Sampler*/, D3DSAMPLERSTATETYPE /*Type*/, DWORD /*Value*/)
	{
		__asm
		{
			mov		edx, [guSSChanges]
			mov		edx, dword ptr [edx]
			jmp		[RenderState_SetSamplerState_JumpBack]
		}
	}

	void RenderState_SetSamplerState_Fallback(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value)
	{
		RenderState_SetSamplerState_Original(Sampler, Type, RenderState_GetFallbackSamplerValue(Type, Value));
	}

	static void (*orgMarkProfiler)(void*);
	void MarkProfiler_SetAF(void* a1)
	{
		orgMarkProfiler(a1);

		const DWORD anisotropicLevel = CMR_GetAnisotropicLevel();
		for (DWORD Sampler = 0; Sampler < 8; ++Sampler)
		{
			RenderState_SetTextureAnisotropicLevel(Sampler, anisotropicLevel);
		}
	}

	static void (*orgSetMipBias)(D3DTexture*, float);
	void Texture_SetMipBiasAndAnisotropic(D3DTexture* texture, float bias)
	{
		orgSetMipBias(texture, bias);
		Core_Texture_SetFilteringMethod(texture, D3DTEXF_ANISOTROPIC, D3DTEXF_ANISOTROPIC, D3DTEXF_LINEAR);
	}

	D3DTEXTUREFILTERTYPE GetAnisotropicFilter()
	{
		return D3DTEXF_ANISOTROPIC;
	}

	static void UpdatePresentationParameters_Internal(const Graphics_Config* config)
	{
		gd3dPP->PresentationInterval = config->m_presentationInterval;
	}

	// Fake name, as different functions are being patched with the same wrapper
	template<std::size_t Index>
	void (*orgUpdatePresentationParameters)(const Graphics_Config*);

	template<std::size_t Index>
	void UpdatePresentationParameters_Hook(const Graphics_Config* config)
	{
		orgUpdatePresentationParameters<Index>(config);
		UpdatePresentationParameters_Internal(config);
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl_UpdatePresentationParameters(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), orgUpdatePresentationParameters<Ctr << 16 | I>, UpdatePresentationParameters_Hook<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach_UpdatePresentationParameters(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl_UpdatePresentationParameters<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}

	int WINAPI GetSystemMetrics_Desktop(int nIndex)
	{
		if (nIndex == SM_CXSCREEN || nIndex == SM_CYSCREEN)
		{
			const int32_t Adapter = Registry::GetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"ADAPTER").value_or(0);
			const RECT& adapterRect = GetAdapterRect(Adapter);
			return nIndex == SM_CYSCREEN ? adapterRect.bottom - adapterRect.top : adapterRect.right - adapterRect.left;
		}
		return ::GetSystemMetrics(nIndex);
	}
	static const auto pGetSystemMetrics_Desktop = &GetSystemMetrics_Desktop;
}

uint32_t CMR_GetNumRefreshRates(const MenuResolutionEntry* entry)
{
	const auto* rates = NewAdvancedGraphicsOptions::GetRefreshRatesForMenuEntry(entry);
	return rates != nullptr ? rates->size() : 0;
}

uint32_t CMR_GetRefreshRateIndex(const MenuResolutionEntry* entry, uint32_t refreshRate)
{
	const auto* rates = NewAdvancedGraphicsOptions::GetRefreshRatesForMenuEntry(entry);
	if (rates == nullptr)
	{
		return 0;
	}

	auto it = std::find(rates->begin(), rates->end(), refreshRate);
	if (it != rates->end())
	{
		return std::distance(rates->begin(), it);
	}
	
	// If refresh rate doesn't exist, get the "top" one
	return std::max(1u, rates->size()) - 1;
}

uint32_t CMR_GetRefreshRateFromIndex(const MenuResolutionEntry* entry, uint32_t index)
{
	const auto* rates = NewAdvancedGraphicsOptions::GetRefreshRatesForMenuEntry(entry);
	if (rates != nullptr && rates->size() > index)
	{
		return (*rates)[index];
	}
	return 0;
}

namespace NewGraphicsOptions
{
	static void* PC_GraphicsOptions_Display_NewOptionsJumpBack;
	__declspec(naked) void PC_GraphicsOptions_Display_CaseNewOptions()
	{
		__asm
		{
			push	dword ptr [esp+228h-214h] // onColor
			push	dword ptr [esp+22Ch-218h] // offColor
			push	ebx // entryID
			push	dword ptr [esp+234h-208h] // posY
			push	dword ptr [esp+238h+8] // interp
			push	dword ptr [esp+23Ch+4] // menu
			call	PC_GraphicsOptions_Display_NewOptions
			jmp		[PC_GraphicsOptions_Display_NewOptionsJumpBack]
		}
	}

	__declspec(naked) void PC_GraphicsOptions_Display_CaseNewOptions_Czech()
	{
		__asm
		{
			push	dword ptr [esp+228h-218h] // onColor
			push	dword ptr [esp+22Ch-214h] // offColor
			push	eax // entryID
			push	dword ptr [esp+234h-208h] // posY
			push	dword ptr [esp+238h+8] // interp
			push	dword ptr [esp+23Ch+4] // menu
			call	PC_GraphicsOptions_Display_NewOptions
			jmp		[PC_GraphicsOptions_Display_NewOptionsJumpBack]
		}
	}

	static float exteriorFOV = static_cast<float>(75.0f * M_PI / 180.0f);
	static float interiorFOV = static_cast<float>(75.0f * M_PI / 180.0f);

	static void* (*orgCameras_AfterInitialise)();
	void* Cameras_AfterInitialise()
	{
		exteriorFOV = static_cast<float>(CMR_FE_GetExteriorFOV() * M_PI / 180.0f);
		interiorFOV = static_cast<float>(CMR_FE_GetInteriorFOV() * M_PI / 180.0f);

		return orgCameras_AfterInitialise();
	}

	void Camera_SetFOV_Exterior(void* camera, float /*FOV*/)
	{
		float* fov = reinterpret_cast<float*>(static_cast<char*>(camera) + 4);
		*fov = exteriorFOV;
	}

	void Camera_SetFOV_Interior(void* camera, float /*FOV*/)
	{
		float* fov = reinterpret_cast<float*>(static_cast<char*>(camera) + 4);
		*fov = interiorFOV;
	}

	static bool VerticalSplitscreen = false;
	uint32_t IsVerticalSplitscreen()
	{
		return VerticalSplitscreen;
	}

	uint32_t IsVerticalSplitscreen_ReadOption()
	{
		VerticalSplitscreen = CMR_FE_GetVerticalSplitscreen();
		return IsVerticalSplitscreen();
	}

	static bool DigitalTacho = false;

	template<std::size_t Index>
	uint8_t (*orgGameInfo_GetNumberOfPlayersInThisRace)();

	template<std::size_t Index>
	uint8_t GameInfo_GetNumberOfPlayersInThisRace_DigitalTachoHack()
	{
		const uint8_t numPlayers = orgGameInfo_GetNumberOfPlayersInThisRace<Index>();
		if (numPlayers == 1 && DigitalTacho)
		{
			return 2;
		}
		return numPlayers;
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl_GetNumberOfPlayersInThisRace(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), orgGameInfo_GetNumberOfPlayersInThisRace<Ctr << 16 | I>, GameInfo_GetNumberOfPlayersInThisRace_DigitalTachoHack<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach_GetNumberOfPlayersInThisRace(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl_GetNumberOfPlayersInThisRace<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}

	template<std::size_t Index>
	bool (*orgGetWidescreen)();

	template<std::size_t Index>
	bool GetWidesreen_DigitalTachoHack()
	{
		// Initialize the option from the first patch
		if constexpr (Index == 0)
		{
			DigitalTacho = CMR_FE_GetDigitalTacho();
		}

		if (DigitalTacho && GameInfo_GetNumberOfPlayersInThisRace() == 1)
		{
			return false;
		}
		return orgGetWidescreen<Index>();
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl_GetWidescreen(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), orgGetWidescreen<Ctr << 16 | I>, GetWidesreen_DigitalTachoHack<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach_GetWidescreen(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl_GetWidescreen<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}

	static void (*orgDrawTacho)(int playerNo, float);
	void DrawTacho_SetPosition(int playerNo, float a2)
	{
		if (DigitalTacho && GameInfo_GetNumberOfPlayersInThisRace() == 1)
		{
			OSD_Data2* PositionInfoMulti = *gpPositionInfoMulti;

			const int OriginalTachoX = std::exchange(PositionInfoMulti->m_TachoX, static_cast<int>(GetScaledResolutionWidth()) - 160);
			const int OriginalTachoY = std::exchange(PositionInfoMulti->m_TachoY, 310);

			orgDrawTacho(playerNo, a2);

			PositionInfoMulti->m_TachoX = OriginalTachoX;
			PositionInfoMulti->m_TachoY = OriginalTachoY;
		}
		else
		{
			orgDrawTacho(playerNo, a2);
		}
	}
}

// Those belong in their cpp files, but they're heavily templated so it's easier to drop them here...
namespace Menus::Patches
{
	template<std::size_t Index>
	void (*orgFrontEndMenuSystem_SetupMenus)(int);

	template<std::size_t Index>
	void FrontEndMenuSystem_SetupMenus(int languagesOnly)
	{
		orgFrontEndMenuSystem_SetupMenus<Index>(languagesOnly);
		FrontEndMenuSystem_SetupMenus_Custom(languagesOnly);
	}

	template<std::size_t Index>
	void (*orgResultsMenuSystem_Initialise)();

	template<std::size_t Index>
	void ResultsMenuSystem_Initialise()
	{
		orgResultsMenuSystem_Initialise<Index>();
		ResultsMenuSystem_Initialise_Custom();
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl_FrontEndMenus(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), orgFrontEndMenuSystem_SetupMenus<Ctr << 16 | I>, FrontEndMenuSystem_SetupMenus<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach_FrontEndMenus(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl_FrontEndMenus<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl_ResultMenus(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), orgResultsMenuSystem_Initialise<Ctr << 16 | I>, ResultsMenuSystem_Initialise<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach_ResultMenus(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl_ResultMenus<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}
}

namespace Graphics::Patches
{
	// Fake name, as different functions are being patched with the same wrapper
	template<std::size_t Index>
	void (*org_Graphics_Config_Func)(Graphics_Config*);

	template<std::size_t Index>
	void Graphics_Config_Func_RecalculateUI(Graphics_Config* config)
	{
		org_Graphics_Config_Func<Index>(config);
		RecalculateUI();
	}

	template<std::size_t Ctr, typename Tuple, std::size_t... I, typename Func>
	void HookEachImpl_RecalculateUI(Tuple&& tuple, std::index_sequence<I...>, Func&& f)
	{
		(f(std::get<I>(tuple), org_Graphics_Config_Func<Ctr << 16 | I>, Graphics_Config_Func_RecalculateUI<Ctr << 16 | I>), ...);
	}

	template<std::size_t Ctr = 0, typename Vars, typename Func>
	void HookEach_RecalculateUI(Vars&& vars, Func&& f)
	{
		auto tuple = std::tuple_cat(std::forward<Vars>(vars));
		HookEachImpl_RecalculateUI<Ctr>(std::move(tuple), std::make_index_sequence<std::tuple_size_v<decltype(tuple)>>{}, std::forward<Func>(f));
	}
}

static void ApplyMergedLocalizations(const bool HasRegistry, const bool HasFrontEnd, const bool HasGameInfo, const bool HasLanguageHook,
			const bool HasKeyboard, const bool HasTexture, const bool HasCMR3Font)
{
	const bool WantsTexts = Version::HasMultipleLocales();
	const bool WantsCoDrivers = Version::HasMultipleCoDrivers();
	const bool WantsNickyGristPatched = Version::HasNickyGristFiles();

	using namespace Memory;
	using namespace hook::txn;

	auto InterceptCall = [](void* addr, auto&& func, auto&& hook)
	{
		ReadCall(addr, func);
		InjectHook(addr, hook);
	};

	// Restored languages in Polish
	if (HasGameInfo && HasRegistry) try
	{
		using namespace Localization;

		auto get_current_language_id = pattern("6A 45 E8 ? ? ? ? C3").get_one();

		// Un-hardcoded English text language
		ReadCall(get_current_language_id.get<void>(2), orgGetLanguageIDByCode);
		InjectHook(get_current_language_id.get<void>(), GetCurrentLanguageID_Patched, PATCH_JUMP);

		// Un-hardcoded co-driver language
		// Patch will probably fail pattern matches on set_defaults when used on the English executable
		if (WantsCoDrivers || WantsNickyGristPatched)
		{
			auto get_codriver_language = ReadCallFrom(get_pattern("E8 ? ? ? ? 83 F8 03 77 40"));
			auto set_codriver_language = ReadCallFrom(get_pattern("E8 ? ? ? ? 8B 4F 24"));
			auto set_defaults = pattern("A2 ? ? ? ? C6 05 ? ? ? ? 01 88 1D").get_one();
			auto unk_on_main_menu_start = get_pattern("75 ? 6A 02 E8 ? ? ? ? 81 C4", 4);

			gCoDriverLanguage = *set_defaults.get<uint8_t*>(5 + 2);
			InjectHook(get_codriver_language, GetCoDriverLanguage, PATCH_JUMP);
			InjectHook(set_codriver_language, SetCoDriverLanguage, PATCH_JUMP);

			//  mov bCoDriverLanguage, 1 -> mov bCoDriverLanguage, al
			Patch(set_defaults.get<void>(5), { 0x90, 0xA2 });
			Patch(set_defaults.get<void>(5 + 2), gCoDriverLanguage);
			Nop(set_defaults.get<void>(5 + 6), 1);

			// Unknown, called once as main menu starts
			InterceptCall(unk_on_main_menu_start, orgSetUnknown, setUnknown_AndCoDriverLanguage);
		}
	}
	TXN_CATCH();


	// Multi7 texts
	if (WantsTexts && HasRegistry) try
	{
		using namespace Localization;

		// Patch localizations first, and then the other locale-specific stuff only if that succeeds
		void* num_languages_int32[] = {
			get_pattern("8D 4C 24 08 BF 05 00 00 00", 4 + 1),
			get_pattern("BF 05 00 00 00 56 E8 ? ? ? ? 46", 1),

			// Credits
			get_pattern("33 F6 BB 05 00 00 00", 2 + 1),
			[] {
				try {
					// EFIGS/Polish
					return get_pattern("33 FF C7 44 24 ? 05 00 00 00 56", 2 + 4);
				} catch (const hook::txn_exception&) {
					// Czech
					return get_pattern("33 ED C7 44 24 ? 05 00 00 00", 2 + 4);
				}
				}(),
		};

		auto language_data1 = pattern("8B 0C 85 ? ? ? ? 8D 34 85").get_one();
		void* language_data[] = {
			get_pattern("8B 04 B5 ? ? ? ? 85 C0 74 ? 56", 3),
			[] {
				try {
					// EFIGS/Polish
					return get_pattern("8B 04 B5 ? ? ? ? 5E 89 44 24 04", 3);
				} catch (const hook::txn_exception&) {
					// Czech
					return get_pattern("8B 0C B5 ? ? ? ? 51 E8 ? ? ? ? 5E", 3);
				}
			}(),
			language_data1.get<void>(3),
			language_data1.get<void>(7 + 3),
			get_pattern("8B 04 B5 ? ? ? ? 85 C0 74 11", 3),
			get_pattern("C7 04 B5 ? ? ? ? 00 00 00 00 5E C2 04 00", 3),
		};

		void* country_initials[] = {
			get_pattern("8B 04 85 ? ? ? ? 6A 00", 3),
		};
		void* get_language_id_by_code;
		try
		{
			// EFIGS/Polish
			get_language_id_by_code = get_pattern("8A 54 24 04 33 C0");
		}
		catch (const hook::txn_exception&)
		{
			// Czech
			get_language_id_by_code = get_pattern("8A 44 24 04 56");
		}

		auto get_language_code = get_pattern("8B 44 24 04 8B 0C 85 ? ? ? ? 8A 01");
		auto set_language_current = get_pattern("8B 46 24 50 E8 ? ? ? ? 6A 0C E8", 4);

		auto credits_files = get_pattern("8B 0C B5 ? ? ? ? 8D 44 24 0C", 3);

		// For credits, use range checks to patch 25+ references to the array
		auto credits_patches_start = get_pattern_uintptr("2B C2 8B 5C 24 18");
		auto credits_patches_end = get_pattern_uintptr("A1 ? ? ? ? 85 C0 7E 1E");
		// Sanity check - if somehow end comes before start, treat it as a failure
		if (credits_patches_start >= credits_patches_end)
		{
			throw hook::txn_exception();
		}
		auto getCreditsPattern = [credits_patches_start, credits_patches_end](void* val)
		{
			uint8_t bytes[4];
			memcpy(bytes, &val, sizeof(bytes));

			const uint8_t mask[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
			return pattern(credits_patches_start, credits_patches_end, std::basic_string_view<uint8_t>(bytes, std::size(bytes)),
				std::basic_string_view<uint8_t>(mask, std::size(mask)));
		};

		auto credits_globals = pattern(credits_patches_start, credits_patches_end, "68 ? ? ? ? 89 3C B5 ? ? ? ? 89 3C B5").get_one();
		auto credits_groups = getCreditsPattern(*credits_globals.get<void*>(5 + 3));
		auto credits_file_data = getCreditsPattern(*credits_globals.get<void*>(12 + 3));

		// Scan immediately and bail out if either scan fails
		if (credits_groups.empty() || credits_file_data.empty())
		{
			throw hook::txn_exception();
		}

		// Font reloading
		try
		{
			using namespace FontReloading;

			// EFIGS/Polish
			auto frontend_fonts_load = pattern("83 FE 0D 7C ? 68 ? ? ? ? E8 ? ? ? ? 50 E8").count(2);

			// Read out FrontEndFonts_Destroy from the first match, then wrap both to make them one-time calls
			FrontEndFonts_Destroy = *frontend_fonts_load.get(0).get<decltype(FrontEndFonts_Destroy)>(5 + 1);
			FrontEndFonts_Load = static_cast<decltype(FrontEndFonts_Load)>(frontend_fonts_load.get(0).get<void>(-0x24));

			std::array<void*, 2> add_destructor = {
				frontend_fonts_load.get(0).get<void>(16),
				frontend_fonts_load.get(1).get<void>(16),
			};
			HookEach_AddDestructor(add_destructor, InterceptCall);
		}
		catch (const hook::txn_exception&)
		{
			using namespace FontReloading;

			// Czech
			auto frontend_fonts_load = pattern("47 81 FE ? ? ? ? 7C ? 68 ? ? ? ? E8 ? ? ? ? 50 E8").count(2);

			// Read out FrontEndFonts_Destroy from the first match, then wrap both to make them one-time calls
			FrontEndFonts_Destroy = *frontend_fonts_load.get(0).get<decltype(FrontEndFonts_Destroy)>(9 + 1);
			FrontEndFonts_Load = static_cast<decltype(FrontEndFonts_Load)>(frontend_fonts_load.get(0).get<void>(-0x27));

			std::array<void*, 2> add_destructor = {
				frontend_fonts_load.get(0).get<void>(20),
				frontend_fonts_load.get(1).get<void>(20),
			};
			HookEach_AddDestructor(add_destructor, InterceptCall);
		}
		InterceptCall(set_language_current, FontReloading::CMR3Language_SetCurrent, FontReloading::CMR3Language_SetCurrent_ReloadFonts);

		for (void* addr : num_languages_int32)
		{
			Patch<int32_t>(addr, NUM_LOCALES);
		}

		for (void* addr : language_data)
		{
			Patch(addr, &gpLanguageData);
		}
		for (void* addr : country_initials)
		{
			Patch(addr, &gpszCountryInitials);
		}
		Patch(credits_files, &szCreditsFiles);

		InjectHook(get_language_code, GetLanguageCode, PATCH_JUMP);
		InjectHook(get_language_id_by_code, GetLanguageIDByCode, PATCH_JUMP);

		credits_groups.for_each_result([](pattern_match match)
		{
			Patch<const void*>(match.get<void>(), &gpCreditsGroups);
		});
		credits_file_data.for_each_result([](pattern_match match)
		{
			Patch<const void*>(match.get<void>(), &gpCreditsFileData);
		});

		Menus::Patches::MultipleTextsPatched = true;

		// Revert measurement systems defaulting to imperial for English, metric otherwise (Polish forced metric)
		try
		{
			auto set_measurement_system = get_pattern("E8 ? ? ? ? 8B 46 4C 33 C9");

			std::array<void*, 3> set_system_places_to_patch = {
				get_pattern("E8 ? ? ? ? E8 ? ? ? ? 50 E8 ? ? ? ? 68"),
				get_pattern("6A 00 E8 ? ? ? ? 6A 01 E8 ? ? ? ? 5F", 2),
				get_pattern("56 E8 ? ? ? ? 56 E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 57", 1),
			};

			auto set_defaults = pattern("E8 ? ? ? ? B0 64").get_one();

			HookEach_SetMeasurementSystem(set_system_places_to_patch, InterceptCall);

			ReadCall(set_measurement_system, orgSetMeasurementSystem_Defaults);
			InjectHook(set_defaults.get<void>(), SetMeasurementSystem_Defaults);
			Nop(set_defaults.get<void>(5 + 2), 6);
		}
		TXN_CATCH();

		// Multilanguage typing input
		if (HasKeyboard && HasGameInfo) try
		{
			auto convert_scan_code_to_char = pattern("53 55 E8 ? ? ? ? 0F BE").count(3);

			convert_scan_code_to_char.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(2), Keyboard_ConvertScanCodeToCharLocalised);
			});
		}
		TXN_CATCH();

		// Restored cube layouts
		if (HasGameInfo && HasTexture) try
		{
			using namespace Cubes;

			std::array<void*, 2> load_cube_textures = {
				get_pattern("E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 8B 44 24 08 6A 0E"),
				get_pattern("E8 ? ? ? ? A1 ? ? ? ? 85 C0 75 14"),
			};

			gBlankCubeTexture = *get_pattern<D3DTexture**>("56 68 ? ? ? ? E8 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 68", 1 + 5 + 5 + 1);

			gGearCubeLayouts = *get_pattern<D3DTexture**>("8B 04 95 ? ? ? ? 50 6A 00 E8 ? ? ? ? 83 E0 03 83 C0 02 50 6A 00", 3);
			gStageCubeLayouts = *get_pattern<D3DTexture**>("8B 14 8D ? ? ? ? 52 6A 00 E8 ? ? ? ? 83 E0 03 83 C0 02 50 6A 02 E8 ? ? ? ? 0F BF 46 18 A3 ? ? ? ? A1", 3) - 2;

			// Those need different treatment in Polish and EFIGS/Czech, so locate them last
			try
			{
				// Polish - no need to patch loading
				gGearCubeTextures = *get_pattern<D3DTexture**>("BE ? ? ? ? BF 07 00 00 00 56", 1);
				gStageCubeTextures = *get_pattern<D3DTexture**>("BE ? ? ? ? BF 09 00 00 00 56", 1);
			}
			catch (const hook::txn_exception&)
			{
				// EFIGS/Czech - patch loading, and point those at our own allocations
				auto gear_cubes_load = pattern("83 FE 0C 7C C1").get_one();
				auto stage_cubes_load = pattern("83 FE 1C 7C C1").get_one();
				auto cubes_destructor = get_pattern("A1 ? ? ? ? 68 ? ? ? ? 89 0D", 5 + 1);

				Patch(gear_cubes_load.get<void>(-0x3A + 2), &gGearCubeNames);
				Patch(gear_cubes_load.get<void>(-0x24 + 2), &gGearCubeNames);
				Patch(gear_cubes_load.get<void>(-0x16 + 2), &gGearCubeTexturesSpace);
				Patch<int8_t>(gear_cubes_load.get<void>(2), 7 * 4);

				Patch(stage_cubes_load.get<void>(-0x3A + 2), &gStageCubeNames);
				Patch(stage_cubes_load.get<void>(-0x24 + 2), &gStageCubeNames);
				Patch(stage_cubes_load.get<void>(-0x16 + 2), &gStageCubeTexturesSpace);
				Patch<int8_t>(stage_cubes_load.get<void>(2), 9 * 4);

				Cubes_Destroy = *static_cast<decltype(Cubes_Destroy)*>(cubes_destructor);
				Patch(cubes_destructor, Cubes_Destroy_Custom);

				gGearCubeTextures = gGearCubeTexturesSpace;
				gStageCubeTextures = gStageCubeTexturesSpace;
			}

			HookEach(load_cube_textures, InterceptCall);
		}
		TXN_CATCH();
	}
	TXN_CATCH();


	// Multi7 languages in the Language screen
	// This is applied unconditionally, even if additional languages are not installed, as it also acts as a bugfix
	// for misplaced menu arrows
	if (HasGameInfo && HasLanguageHook && HasFrontEnd) try
	{
		using namespace ConsistentLanguagesScreen;

		auto codriver2_1 = pattern("E8 ? ? ? ? 83 C4 10 68 ? ? ? ? EB").count(2);
		void* codriver2[] = {
			codriver2_1.get(0).get<void>(),
			codriver2_1.get(1).get<void>(),
			get_pattern("E8 ? ? ? ? 8B 4E 4C 83 C4 10 81 C1"),
			get_pattern("E8 ? ? ? ? 83 C4 10 68 ? ? ? ? E9"),
		};

		auto codriver1 = get_pattern("E8 ? ? ? ? 83 C4 0C 68 ? ? ? ? 6A 00 E8 ? ? ? ? 03 C7 55 8B 54 24 20");

		auto lang2 = get_pattern("E8 ? ? ? ? 8B 46 24 83 C4 10");
		auto lang1 = get_pattern("E8 ? ? ? ? 83 C4 0C 68 ? ? ? ? 6A 00 E8 ? ? ? ? 03 C7 55 8B 4C 24 20");

		InjectHook(codriver1, sprintf_codriver1);
		for (void* addr : codriver2)
		{
			InjectHook(addr, sprintf_codriver2);
		}

		InjectHook(lang2, sprintf_lang2);
		InjectHook(lang1, sprintf_lang1);
	}
	TXN_CATCH();


	// Secrets screen
	// Applied unconditionally (for the most part) because it makes the secrets screen more consistent
	if (HasGameInfo && HasCMR3Font && HasLanguageHook) try
	{
		using namespace SecretsScreen;

		// Secrets screen
		// The amount of texts to patch differs between executables, so a range check is needed
		std::vector<void*> blits_to_nop;

		auto secrets_begin = get_pattern_uintptr("C7 05 ? ? ? ? 80 02 00 00 D9 44 24 20");
		auto secrets_end = get_pattern_uintptr("89 44 24 1C 3B FA");
		pattern(secrets_begin, secrets_end, "68 40 01 00 00 68 ? ? ? ? E8 ? ? ? ? 50 6A ? E8").for_each_result([&](pattern_match match)
			{
				blits_to_nop.emplace_back(match.get<void>(18));
			});
		pattern(secrets_begin, secrets_end, "68 40 01 00 00 68 ? ? ? ? 6A ? E8").for_each_result([&](pattern_match match)
			{
				blits_to_nop.emplace_back(match.get<void>(12));
			});
		pattern(secrets_begin, secrets_end, "6A 3D 68 ? ? ? ? E8 ? ? ? ? 50 6A ? E8").for_each_result([&](pattern_match match)
			{
				blits_to_nop.emplace_back(match.get<void>(15));
			});

		// Intercept this BlitText only to get alpha from it
		auto blit_to_intercept = pattern(secrets_begin, secrets_end, "6A 0C E8 ? ? ? ? 6A 21").get_first<void>(2);

		auto get_access_code = ReadCallFrom(pattern(secrets_begin, secrets_end, "E8 ? ? ? ? E8 ? ? ? ? 50 68 ? ? ? ? 68").get_first<void>(5));

		// Only bother with scrollers if the language pack got patched in
		if (Menus::Patches::MultipleTextsPatched)
		{
			auto cheat_menu_enter = pattern("68 ? ? ? ? 55 C7 05 ? ? ? ? ? ? ? ? E8 ? ? ? ? 68 ? ? ? ? 55 A3 ? ? ? ? E8 ? ? ? ? 68").get_one();

			auto cheat_menu_display1 = pattern(secrets_begin, secrets_end, "68 49 01 00 00 ? 68").count(2);
			auto cheat_menu_display_german = pattern(secrets_begin, secrets_end, "68 49 01 00 00 ? ? ? 68").get_first<void>(5 + 3 + 1);

			Patch(cheat_menu_enter.get<void>(1), &gEnglishCallsTextBuffer);
			Patch(cheat_menu_display1.get(0).get<void>(6 + 1), &gEnglishCallsTextBuffer);

			Patch(cheat_menu_enter.get<void>(0x15 + 1), &gFrenchCallsTextBuffer);
			Patch(cheat_menu_display1.get(1).get<void>(6 + 1), &gFrenchCallsTextBuffer);

			Patch(cheat_menu_enter.get<void>(0x25 + 1), &gGermanCallsTextBuffer);
			Patch(cheat_menu_display_german, &gGermanCallsTextBuffer);

			InterceptCall(cheat_menu_enter.get<void>(0x10), orgCMR3Font_GetTextWidth, CMR3Font_GetTextWidth_Scroller);
		}

		GetAccessCode = static_cast<decltype(GetAccessCode)>(get_access_code);
		for (void* addr : blits_to_nop)
		{
			InjectHook(addr, CMR3Font_BlitText_NOP);
		}

		InjectHook(blit_to_intercept, CMR3Font_BlitText_DrawWrapper);
	}
	TXN_CATCH();


	// More translatable strings
	// Can be applied unconditionally because it uses safe fallbacks
	if (HasLanguageHook)
	{
		using namespace MoreTranslatableStrings;

		// Patch each case separately since different localized executables may or may not have those already
		try
		{
			// NA in Telemetry
			auto na_string = get_pattern("E8 ? ? ? ? 83 C4 10 EB 31");
			InjectHook(na_string, sprintf_na);
		}
		TXN_CATCH();

		try
		{
			// Localized key names
			auto get_keyboard_key_name = get_pattern("57 8B 7C 24 0C 6A 00 6A 00", -5);
			auto get_letter_name = ReadCallFrom(get_pattern("C6 06 00 E8 ? ? ? ? 84 C0", 3));

			Keyboard_ConvertScanCodeToChar = static_cast<decltype(Keyboard_ConvertScanCodeToChar)>(get_letter_name);
			InjectHook(get_keyboard_key_name, Keyboard_ConvertScanCodeToString, PATCH_JUMP);
		}
		TXN_CATCH();

		try
		{
			// "Return to Centre", previously hardcoded everywhere
			auto centre1 = get_pattern("E8 ? ? ? ? 83 C4 0C 68 ? ? ? ? 55");
			auto centre2 = get_pattern("E8 ? ? ? ? 55 68 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 83 C4 1C");

			InjectHook(centre1, sprintf_returntocentre1);
			InjectHook(centre2, sprintf_returntocentre2);
		}
		TXN_CATCH();
	}


	// Multi7 boot screen (if present)
	if (HasGameInfo && Version::HasMultipleBootScreens()) try
	{
		using namespace Localization::BootScreen;

		auto set_current_directory_boot_screen = get_pattern("E8 ? ? ? ? 53 68 ? ? ? ? 68 ? ? ? ? 68");
		InterceptCall(set_current_directory_boot_screen, orgFile_SetCurrentDirectory, File_SetCurrentDirectory_BootScreen);
	}
	TXN_CATCH();


	// Multi7 fonts
	// Applied unconditionally due to the HD UI that must be able to load from fonts_P/fonts_C even without the locale pack
	try
	{
		auto fonts_load = get_pattern("E8 ? ? ? ? 83 C4 0C 8D 4C 24 0C");
		InjectHook(fonts_load, Localization::sprintf_RegionalFont);
	}
	TXN_CATCH();


	// Multi7 co-drivers
	if (HasGameInfo && WantsCoDrivers) try
	{
		using namespace Localization;

		void* sprintf_cod;
		try
		{
			// EFIGS/Polish
			sprintf_cod = get_pattern("E8 ? ? ? ? 83 C4 0C 68 ? ? ? ? E8 ? ? ? ? 8D 54 24 04");
		}
		catch (const hook::txn_exception&)
		{
			sprintf_cod = get_pattern("52 E8 ? ? ? ? 83 C4 0C 68", 1);
		}

		// Only do this if Nicky Grist files are absent
		if (!WantsNickyGristPatched)
		{
			auto language_init_codriver = get_pattern("E8 ? ? ? ? 25 ? ? ? ? 89 46 4C");
			auto language_exit_codriver = get_pattern("E8 ? ? ? ? 8B ? 4C 50 E8", 5+4);

			InterceptCall(language_init_codriver, orgGameInfo_GetCoDriverLanguage_NoNickyGrist, GameInfo_GetCoDriverLanguage_NoNickyGrist);
			InterceptCall(language_exit_codriver, orgGameInfo_SetCoDriverLanguage_NoNickyGrist, GameInfo_SetCoDriverLanguage_NoNickyGrist);
		}

		InjectHook(sprintf_cod, Localization::sprintf_cod);

		Menus::Patches::MultipleCoDriversPatched = true;
	}
	TXN_CATCH();
}


static void ApplyPatches(const bool HasRegistry)
{
	static_assert(std::string_view(__FUNCSIG__).find("__stdcall") != std::string_view::npos, "This codebase must default to __stdcall, please change your compilation settings.");

	using namespace Memory;
	using namespace hook::txn;

	// This may be overwritten later
	DesktopRects.push_back({ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) });

	const HINSTANCE mainModuleInstance = GetModuleHandle(nullptr);

	auto Protect = ScopedUnprotect::UnprotectSectionOrFullModule(mainModuleInstance, ".text");

	auto InterceptCall = [](void* addr, auto&& func, auto&& hook)
	{
		ReadCall(addr, func);
		InjectHook(addr, hook);
	};

	// Globally replace timeGetTime with a QPC-based timer
	Timers::Setup();
	Timers::RedirectImports();

	// Locate globals later patches might rely on
	bool HasGlobals = false;
	try
	{
		gszTempString = *get_pattern<char*>("68 ? ? ? ? E8 ? ? ? ? 8B 46 4C", 1);

		HasGlobals = true;
	}
	TXN_CATCH();

	bool HasDestruct = false;
	try
	{
		auto get_destructor = pattern("5F 5E B8 ? ? ? ? 5B 83 C4 28").get_one();

		ReadCall(get_destructor.get<void>(-11), Destruct_GetCoreDestructorGroup);
		ReadCall(get_destructor.get<void>(-5), Destruct_AddDestructor);

		HasDestruct = true;
	}
	TXN_CATCH();

	bool HasCored3d = false;
	try
	{
		auto check_for_device_lost = pattern("68 ? ? ? ? 50 FF 52 40").get_one();
		auto caps = *get_pattern<D3DCAPS9*>("BF ? ? ? ? F3 A5 A1 ? ? ? ? 85 C0", 1);
		auto d3d = *get_pattern<IDirect3D9**>("8B 0D ? ? ? ? 56 8B 74 24 0C", 2);

		gd3dPP = *check_for_device_lost.get<D3DPRESENT_PARAMETERS*>(1);
		gpd3dDevice = *check_for_device_lost.get<IDirect3DDevice9**>(-7 + 1);
		gD3DCaps = caps;
		gpD3D = d3d;

		HasCored3d = true;
	}
	TXN_CATCH();

	bool HasCMR3FE = false;
	try
	{
		auto funcs_save = pattern("E8 ? ? ? ? 8A 15 ? ? ? ? 52 E8 ? ? ? ? A1 ? ? ? ? 50").get_one();
		auto funcs_load = pattern("E8 ? ? ? ? 25 ? ? ? ? A3 ? ? ? ? E8 ? ? ? ? A3 ? ? ? ? E8 ? ? ? ? DB 05 ? ? ? ? 51 A3").get_one();

		CMR_FE_GetTextureQuality = reinterpret_cast<decltype(CMR_FE_GetTextureQuality)>(ReadCallFrom(funcs_load.get<void>(0)));
		CMR_FE_GetEnvironmentMap = reinterpret_cast<decltype(CMR_FE_GetEnvironmentMap)>(ReadCallFrom(funcs_load.get<void>(15)));
		CMR_FE_GetDrawShadow = reinterpret_cast<decltype(CMR_FE_GetDrawShadow)>(ReadCallFrom(funcs_load.get<void>(25)));
		CMR_FE_GetGraphicsQuality = reinterpret_cast<decltype(CMR_FE_GetGraphicsQuality)>(ReadCallFrom(get_pattern("E8 ? ? ? ? 25 ? ? ? ? 33 C9")));
		CMR_FE_GetDrawDistance = reinterpret_cast<decltype(CMR_FE_GetDrawDistance)>(ReadCallFrom(get_pattern("E8 ? ? ? ? 89 44 24 24 DB 44 24 24")));

		CMR_FE_SetGraphicsQuality = reinterpret_cast<decltype(CMR_FE_SetTextureQuality)>(ReadCallFrom(funcs_save.get<void>(0)));
		CMR_FE_SetTextureQuality = reinterpret_cast<decltype(CMR_FE_SetTextureQuality)>(ReadCallFrom(funcs_save.get<void>(5+6+1)));
		CMR_FE_SetEnvironmentMap = reinterpret_cast<decltype(CMR_FE_SetEnvironmentMap)>(ReadCallFrom(funcs_save.get<void>(0x17)));
		CMR_FE_SetDrawShadow = reinterpret_cast<decltype(CMR_FE_SetDrawShadow)>(ReadCallFrom(funcs_save.get<void>(0x23)));
		CMR_FE_SetDrawDistance = reinterpret_cast<decltype(CMR_FE_SetDrawDistance)>(ReadCallFrom(funcs_save.get<void>(0x4C)));
		CMR_FE_SetFSAA = reinterpret_cast<decltype(CMR_FE_SetFSAA)>(ReadCallFrom(funcs_save.get<void>(0x2F)));

		CMR_FE_StoreRegistry = reinterpret_cast<decltype(CMR_FE_StoreRegistry)>(ReadCallFrom(funcs_save.get<void>(-0xB)));

		SetUseLowQualityTextures = reinterpret_cast<decltype(SetUseLowQualityTextures)>(ReadCallFrom(get_pattern("F6 D8 1B C0 40 50 E8 ? ? ? ? E8", 6)));

		DrawLeftRightArrows = reinterpret_cast<decltype(DrawLeftRightArrows)>(get_pattern("81 EC ? ? ? ? 0F BF 41 18", -8));

		HasCMR3FE = true;
	}
	TXN_CATCH();

	bool HasCMR3Font = false;
	try
	{
		CMR3Font_BlitText = reinterpret_cast<decltype(CMR3Font_BlitText)>(get_pattern("8B 74 24 30 8B 0D", -6));
		CMR3Font_GetTextWidth = reinterpret_cast<decltype(CMR3Font_GetTextWidth)>(get_pattern("25 FF 00 00 00 55 56 57 8D 0C C5 00 00 00 00", -0xD));

		HasCMR3Font = true;
	}
	TXN_CATCH();

	bool HasGraphics = false;
	try
	{
		auto set_gamma_ramp = get_pattern("D9 44 24 08 81 EC");
		auto get_num_adapters = ReadCallFrom(get_pattern("E8 ? ? ? ? 8B F8 32 DB"));
		//auto get_num_modes = ReadCallFrom(get_pattern("55 E8 ? ? ? ? 33 C9 3B C1 89 44 24", 1));
		auto get_mode = ReadCallFrom(get_pattern("E8 ? ? ? ? 8B F0 32 C9"));
		auto get_current_config = reinterpret_cast<uintptr_t>(ReadCallFrom(get_pattern("E8 ? ? ? ? 51 8B 7C 24 68")));
		auto check_for_vertex_shaders = [] {
			try
			{
				return get_pattern("81 EC ? ? ? ? 8D 8C 24", -4);
			}
			catch (const hook::txn_exception&)
			{
				// Czech EXE has a bigger stack in this function
				return get_pattern("81 EC ? ? ? ? 8D 4C 24 00 56", -4);
			}
		}();

		auto setup_render = get_pattern("81 EC ? ? ? ? 56 8D 44 24 08");
		auto get_adapter_caps = get_pattern("8B 08 81 EC ? ? ? ? 56", -5);
		auto get_resolution_entry = ReadCallFrom(get_pattern("A1 ? ? ? ? 52 50 E8 ? ? ? ? 8B F0 56", 7));

		auto save_func = pattern("E8 ? ? ? ? 8B 0D ? ? ? ? 6A FF 51 8B F8 E8").get_one();
		auto get_modes = pattern("E8 ? ? ? ? 33 F6 85 C0 89 44 24 14").get_one();

		auto get_screen_width = ReadCallFrom(get_pattern("E8 ? ? ? ? 33 F6 89 44 24 28"));
		auto get_screen_height = ReadCallFrom(get_pattern("E8 ? ? ? ? 8D 4C 6D 00"));

		Graphics_SetGammaRamp = reinterpret_cast<decltype(Graphics_SetGammaRamp)>(set_gamma_ramp);
		Graphics_GetNumAdapters = reinterpret_cast<decltype(Graphics_GetNumAdapters)>(get_num_adapters);
		//Graphics_GetNumModes = reinterpret_cast<decltype(Graphics_GetNumModes)>(get_num_modes);
		Graphics_GetMode = reinterpret_cast<decltype(Graphics_GetMode)>(get_mode);
		Graphics_CheckForVertexShaders = reinterpret_cast<decltype(Graphics_CheckForVertexShaders)>(check_for_vertex_shaders);
		Graphics_GetAdapterCaps = reinterpret_cast<decltype(Graphics_GetAdapterCaps)>(get_adapter_caps);

		Graphics_GetScreenWidth = reinterpret_cast<decltype(Graphics_GetScreenWidth)>(get_screen_width);
		Graphics_GetScreenHeight = reinterpret_cast<decltype(Graphics_GetScreenHeight)>(get_screen_height);
		GetMenuResolutionEntry = reinterpret_cast<decltype(GetMenuResolutionEntry)>(get_resolution_entry);

		CMR_GetAdapterProductID = reinterpret_cast<decltype(CMR_GetAdapterProductID)>(ReadCallFrom(save_func.get<void>(0)));
		CMR_GetAdapterVendorID = reinterpret_cast<decltype(CMR_GetAdapterVendorID)>(ReadCallFrom(save_func.get<void>(0x10)));

		CMR_GetValidModeIndex = reinterpret_cast<decltype(CMR_GetValidModeIndex)>(get_modes.get<void>(-9));
		CMR_ValidateModeFormats = reinterpret_cast<decltype(CMR_ValidateModeFormats)>(ReadCallFrom(get_modes.get<void>(0)));

		CMR_SetupRender = reinterpret_cast<decltype(CMR_SetupRender)>(setup_render);

		gGraphicsConfig = *reinterpret_cast<Graphics_Config**>(get_current_config + 0xC);

		HasGraphics = true;
	}
	TXN_CATCH();

	bool HasViewport = false;
	try
	{
		auto set_aspect_ratio = get_pattern("8B 44 24 04 85 C0 75 1C");
		auto viewports = *get_pattern<D3DViewport**>("8B 35 ? ? ? ? 8B C6 5F", 2);
		auto full_screen_viewport = *get_pattern<D3DViewport**>("A1 ? ? ? ? D9 44 24 08 D9 58 1C", 1);
		auto current_viewport = *get_pattern<D3DViewport**>("A3 ? ? ? ? 8B 48 54", 1);

		gpFullScreenViewport = full_screen_viewport;
		gpCurrentViewport = current_viewport;
		Viewport_SetAspectRatio = reinterpret_cast<decltype(Viewport_SetAspectRatio)>(set_aspect_ratio);
		gViewports = viewports;

		HasViewport = true;
	}
	TXN_CATCH();

	bool HasHandyFunction = false;
	try
	{
		auto draw_2d_box = get_pattern("6A 01 E8 ? ? ? ? 6A 05 E8 ? ? ? ? 6A 06 E8 ? ? ? ? DB 44 24 5C", -5);
		auto draw_2d_line_from_to = get_pattern("8B 54 24 54 89 44 24 10", -0xB);
		auto clip_2d_rect = get_pattern("DF E0 25 ? ? ? ? 75 0E D9 41 10 D8 5C 24 10", -0xB);

		HandyFunction_Draw2DBox = reinterpret_cast<decltype(HandyFunction_Draw2DBox)>(draw_2d_box);
		HandyFunction_Draw2DLineFromTo = reinterpret_cast<decltype(HandyFunction_Draw2DLineFromTo)>(draw_2d_line_from_to);
		HandyFunction_Clip2DRect = reinterpret_cast<decltype(HandyFunction_Clip2DRect)>(clip_2d_rect);

		HasHandyFunction = true;
	}
	TXN_CATCH();

	bool HasBlitter2D = false;
	try
	{
		auto blitter2d_rect2d_g = ReadCallFrom(get_pattern("E8 ? ? ? ? 8D 44 24 68"));
		auto blitter2d_rect2d_gt = ReadCallFrom(get_pattern("DD D8 E8 ? ? ? ? 8B 7C 24 30", 2));
		auto blitter2d_line2d_g = get_pattern("F7 D8 57", -0x14);

		Core_Blitter2D_Rect2D_G = reinterpret_cast<decltype(Core_Blitter2D_Rect2D_G)>(blitter2d_rect2d_g);
		Core_Blitter2D_Rect2D_GT = reinterpret_cast<decltype(Core_Blitter2D_Rect2D_GT)>(blitter2d_rect2d_gt);
		Core_Blitter2D_Line2D_G = reinterpret_cast<decltype(Core_Blitter2D_Line2D_G)>(blitter2d_line2d_g);

		HasBlitter2D = true;
	}
	TXN_CATCH();

	bool HasRenderState = false;
	try
	{
		RenderState_SetSamplerState = reinterpret_cast<decltype(RenderState_SetSamplerState)>(ReadCallFrom(get_pattern("E8 ? ? ? ? D9 44 24 0C D9 1C B5 ? ? ? ? D9 44 24 08")));
		CurrentRenderState = *get_pattern<RenderState*>("8B 0D ? ? ? ? A1 ? ? ? ? 8B 10 51 6A 07", 2);

		// Facade initialization
		const uintptr_t RenderStateStart = reinterpret_cast<uintptr_t>(CurrentRenderState);

		auto set_default = pattern("89 1C B5 ? ? ? ? 8B 0C B5").get_one();
		auto emissive_source = *get_pattern<uintptr_t>("68 94 00 00 00 E8 ? ? ? ? 89 3D", 10 + 2);
		auto blend_op = *get_pattern<uintptr_t>("FF 91 E4 00 00 00 A1 ? ? ? ? 5F", 6 + 1);

		RenderStateFacade::OFFS_emissiveSource = emissive_source - RenderStateStart;
		RenderStateFacade::OFFS_blendOp = blend_op - RenderStateStart;
		RenderStateFacade::OFFS_maxAnisotropy = *set_default.get<uintptr_t>(7 + 3) - RenderStateStart;
		RenderStateFacade::OFFS_borderColor = *set_default.get<uintptr_t>(3) - RenderStateStart;

		HasRenderState = true;
	}
	TXN_CATCH();

	bool HasKeyboard = false;
	try
	{
		auto draw_text_entry_box = get_pattern("56 3B C3 57 0F 84 ? ? ? ? DB 84 24", -0xD);
		auto keyboard_data = *get_pattern<uint8_t*>("81 E2 FF 00 00 00 88 90 ? ? ? ? 40", 6 + 2);

		Keyboard_DrawTextEntryBox = reinterpret_cast<decltype(Keyboard_DrawTextEntryBox)>(draw_text_entry_box);
		gKeyboardData = keyboard_data;

		HasKeyboard = true;
	}
	TXN_CATCH();

	bool HasGameInfo = false;
	try
	{
		auto get_num_players = ReadCallFrom(get_pattern("E8 ? ? ? ? 88 44 24 23"));
		auto get_codriver_language = ReadCallFrom(get_pattern("E8 ? ? ? ? 85 C0 75 45 8D 44 24 08"));

		// Different pattern for EFIGS/Czech and Polish
		void* get_text_language;
		try
		{
			get_text_language = get_pattern("E8 ? ? ? ? 88 44 24 00", -0xB);
		}
		catch (const hook::txn_exception&)
		{
			get_text_language = get_pattern("6A 45 E8 ? ? ? ? C3");
		}
		GameInfo_GetNumberOfPlayersInThisRace = reinterpret_cast<decltype(GameInfo_GetNumberOfPlayersInThisRace)>(get_num_players);
		GameInfo_GetTextLanguage = static_cast<decltype(GameInfo_GetTextLanguage)>(get_text_language);
		GameInfo_GetCoDriverLanguage = static_cast<decltype(GameInfo_GetCoDriverLanguage)>(get_codriver_language);

		HasGameInfo = true;
	}
	TXN_CATCH();

	bool HasUIPositions = false;
	try
	{
		auto position_into_multi = *get_pattern<OSD_Data2**>("A1 ? ? ? ? 8B 50 60 8B 48 64", 1);

		gpPositionInfoMulti = position_into_multi;

		HasUIPositions = true;
	}
	TXN_CATCH();

	bool HasFrontEnd = false;
	try
	{
		auto menus = *get_pattern<MenuDefinition*>("C7 05 ? ? ? ? ? ? ? ? 89 3D ? ? ? ? 89 35", 2+4);
		auto results_menus = *get_pattern<MenuDefinition*>("5D B8 ? ? ? ? 5B 83 C4 08 C2 0C 00", 1+1);
		//auto current_menu = *get_pattern<MenuDefinition**>("89 44 24 ? A1 ? ? ? ? 3D", 4+1);

		gmoFrontEndMenus = menus;
		gmoResultsMenus = results_menus;
		//gpCurrentMenu = current_menu;

		HasFrontEnd = true;
	}
	TXN_CATCH();

	bool HasTexture = false;
	try
	{
		auto texture_destroy = get_pattern("33 F6 3B 3C B5", -6);
		Core_Texture_Destroy = static_cast<decltype(Core_Texture_Destroy)>(texture_destroy);
	
		HasTexture = true;
	}
	TXN_CATCH();

	// Texture replacements
	try
	{
		using namespace ScaledTexturesSupport;

		std::array<void*, 3> load_texture = {
			get_pattern("E8 ? ? ? ? 89 06 5E C2 0C 00"),
			get_pattern("E8 ? ? ? ? 56 89 07"),
			[] {
				try
				{
					// Polish/EFIGS
					return get_pattern("E8 ? ? ? ? 89 07 5F");
				}
				catch (const hook::txn_exception&)
				{
					// Czech
					return get_pattern("E8 ? ? ? ? 89 45 00 5D");
				}
			}()
		};
		auto load_font = pattern("57 E8 ? ? ? ? 8B 0D ? ? ? ? 6A 01").get_one();
		auto platformise_texture_filename = get_pattern("E8 ? ? ? ? 8D 54 24 0C 6A 00");

		HookEach_Misc_Scaled(load_texture, InterceptCall);

		InterceptCall(load_font.get<void>(1), orgCreateTexture_Font, CreateTexture_Font_Scaled);
		InterceptCall(platformise_texture_filename, PlatformiseTextureFilename, PlatformiseTextureFilename_GetScale);

		// NOP Core::Texture_SetFilteringMethod for fonts as we handle it in the above function now
		InjectHook(load_font.get<void>(0x1F), Texture_SetFilteringMethod_NOP);
	}
	TXN_CATCH();


	// Added range check for localizations + new strings added in the Polish release
	bool HasLanguageHook = false;
	try
	{
		using namespace Localization;

		auto get_localized_string = ReadCallFrom(get_pattern("E8 ? ? ? ? 8D 56 E2"));

		gpCurrentLanguage = *get_pattern<LangFile**>("89 0D ? ? ? ? B8 ? ? ? ? 5E", 2);
		InjectHook(get_localized_string, Language_GetString, PATCH_JUMP);

		HasLanguageHook = true;
	}
	TXN_CATCH();


	// Menu changes
	bool HasMenuHook = false;
	if (HasFrontEnd && HasGameInfo) try
	{
		using namespace Menus::Patches;

		std::array<void*, 3> update_menu_entries = {
			[] {
				try
				{
					// EFIGS/Polish
					return get_pattern("6A 01 E8 ? ? ? ? 5F 5E C2 08 00", 2);
				}
				catch (const hook::txn_exception&)
				{
					// Czech
					return get_pattern("6A 01 E8 ? ? ? ? 8B 54 24 10", 2);
				}
			}(),
			get_pattern("E8 ? ? ? ? 6A 00 E8 ? ? ? ? E8 ? ? ? ? C2 08 00", 5 + 2),
			get_pattern("E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 50 E8 ? ? ? ? 50"),
		};

		std::array<void*, 2> update_results_menu_entries = {
			get_pattern("E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? C7 05"),
			get_pattern("89 15 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 85 C0", 6),
		};

		gnCurrentAdapter = *get_pattern<int*>("A3 ? ? ? ? 56 50", 1);
		PC_GraphicsAdvanced_PopulateFromCaps = reinterpret_cast<decltype(PC_GraphicsAdvanced_PopulateFromCaps)>([] {
			try
			{
				return get_pattern("8D 84 24 ? ? ? ? 53 55", -6);
			}
			catch (const hook::txn_exception&)
			{
				// Czech EXE has a bigger stack in this function
				return get_pattern("8D 44 24 ? 53 55 8B AC 24", -6);
			}
		}());

		HookEach_FrontEndMenus(update_menu_entries, InterceptCall);
		HookEach_ResultMenus(update_results_menu_entries, InterceptCall);
		HasMenuHook = true;

		// This only matches in the Polish executable
		try
		{
			// Don't override focus every time menus are updated
			auto set_focus_on_lang_screen = get_pattern("89 0D ? ? ? ? 66 89 35", 6);
			Nop(set_focus_on_lang_screen, 14);
		}
		TXN_CATCH();
	}
	TXN_CATCH();


	// Fix sun flickering with multisampling enabled, and make it fully async to avoid a GPU flush and stutter every time sun shows onscreen
	try
	{
		using namespace OcclusionQueries;

		auto mul_struct_size = get_pattern("C1 E0 04 50 C7 05 ? ? ? ? ? ? ? ? C7 05");
		auto push_struct_size = get_pattern("C7 05 ? ? ? ? ? ? ? ? E8 ? ? ? ? 8B 15 ? ? ? ? 6A 00 50 6A 10", 25);
		auto issue_begin = get_pattern("56 8B 74 24 08 8B 46 04 6A 02");
		auto issue_end = get_pattern("56 8B 74 24 08 8B 46 04 8B 08");
		auto get_data = get_pattern("E8 ? ? ? ? 3B C7 89 44 24 18");
		auto update_state_if_finished = get_pattern("E8 ? ? ? ? 83 F8 03 75 EF");

		auto is_query_idle = pattern("E8 ? ? ? ? 83 F8 03 74 10").get_one();
		auto issue_query_return = get_pattern("E8 ? ? ? ? 8B C3 5F", 5);

		auto calculate_color_from_occlusion = get_pattern("E8 ? ? ? ? 53 56 57 6A 03");

		auto load_horizon = pattern("A3 ? ? ? ? E8 ? ? ? ? 50 E8 ? ? ? ? 5E 8B E5").get_one();
		auto destroy_horizon = get_pattern("50 E8 ? ? ? ? A1 ? ? ? ? 3B C6 74 0C", 1);
		auto visible_sun = *get_pattern<float*>("D9 05 ? ? ? ? D8 1D ? ? ? ? DF E0 F6 C4 44 7B 0B", 2);

		auto render_game_common1 = pattern("57 33 FF 57 E8 ? ? ? ? 57 E8 ? ? ? ? 57 E8 ? ? ? ? 6A 01").get_one();

		// shl eax, 4 -> imul eax, sizeof(OcclusionQuery)
		Patch(mul_struct_size, {0x6B, 0xC0, sizeof(OcclusionQuery)});
		Patch<uint8_t>(push_struct_size,  sizeof(OcclusionQuery));

		InjectHook(issue_begin, OcclusionQuery_IssueBegin, PATCH_JUMP);
		InjectHook(issue_end, OcclusionQuery_IssueEnd, PATCH_JUMP);
		InjectHook(get_data, OcclusionQuery_GetDataScaled);
		InjectHook(update_state_if_finished, OcclusionQuery_UpdateStateIfFinished);

		// while (D3DQuery_UpdateState(pOcclusionQuery) != 3 && D3DQuery_UpdateState(pOcclusionQuery))
		// ->
		// if (!OcclusionQuery_IsIdle(pOcclusionQuery)) return 1;
		InjectHook(is_query_idle.get<void>(0), OcclusionQuery_IsIdle);
		InjectHook(is_query_idle.get<void>(5 + 3 + 2), issue_query_return, PATCH_JUMP);

		InterceptCall(calculate_color_from_occlusion, orgCalculateSunColorFromOcclusion, CalculateSunColorFromOcclusion_Clamped);

		// One occlusion query per viewport
		gpSunOcclusion_Original = *load_horizon.get<OcclusionQuery**>(1);
		gfVisibleSun_Original = visible_sun;

		InterceptCall(load_horizon.get<void>(-10), orgOcclusion_Create, Occlusion_Create_Hooked);
		InterceptCall(destroy_horizon, orgOcclusion_Destroy, Occlusion_Destroy_Hooked);

		Render_RenderGameCommon1_JumpBack = render_game_common1.get<void>();
		InjectHook(render_game_common1.get<void>(-5), Render_RenderGameCommon1_SwitchOcclusion, PATCH_JUMP);
	}
	TXN_CATCH();


	// Slightly more precise timers, not dividing frequency
	try
	{
		using namespace Timers;

		auto get_time_in_ms = Memory::ReadCallFrom(get_pattern("E8 ? ? ? ? EB 3E"));
		InjectHook(get_time_in_ms, GetTimeInMS, PATCH_JUMP);
	}
	TXN_CATCH();


	// Backported fix from CMR04 for timeGetTime not having enough timer resolution for physics updates in splitscreen
	try
	{
		using namespace Timers;

		auto gettime_reset = get_pattern("FF 15 ? ? ? ? 8B F8 8B D0 8B C8", 2);
		auto gettime_update = get_pattern("FF 15 ? ? ? ? A3 ? ? ? ? E8 ? ? ? ? 83 F8 01 5D", 2);

		Patch(gettime_reset, &pTimeGetTime_Reset);
		Patch(gettime_update, &pTimeGetTime_Update);
	}
	TXN_CATCH();


	// Unlocked all resolutions and a 128 resolutions limit lifted
	try
	{
		using namespace ResolutionsList;

		auto check_aspect_ratio = get_pattern("8D 04 40 3B C2 74 05", 5);
		auto get_display_mode_count = get_pattern("55 E8 ? ? ? ? 33 C9 3B C1 89 44 24", 1);

		// Populate the list of addresses to patch with addresses and offsets
		// Those act as "optional", don't fail the entire change if any patterns fail
		// Instead, we'll limit the game to 128 resolutions in GetDisplayModeCount_RelocateArray 
		try
		{
			// To save effort, build patterns manually
			auto func_start = get_pattern_uintptr("8B 4C 24 18 51 55");
			auto func_end = get_pattern_uintptr("8B 4E E8 8B 56 E4");
			uint8_t* resolutions_list = *get_pattern<uint8_t*>("33 C9 85 C0 76 65 BF", 7) - offsetof(MenuResolutionEntry, m_format);

			auto patch_field = [resolutions_list, func_start, func_end](size_t offset)
			{
				uint8_t* ptr = resolutions_list+offset;

				const uint8_t mask[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
				uint8_t bytes[4];
				memcpy(bytes, &ptr, sizeof(ptr));

				pattern(func_start, func_end,
					std::basic_string_view<uint8_t>(bytes, sizeof(bytes)), std::basic_string_view<uint8_t>(mask, sizeof(mask)))
					.for_each_result([&offset](pattern_match match)
				{
					placesToPatch.emplace_back(match.get<void>(0), offset);
				});
			};

			patch_field(offsetof(MenuResolutionEntry, m_width));
			patch_field(offsetof(MenuResolutionEntry, m_height));
			patch_field(offsetof(MenuResolutionEntry, m_refreshRate));
			patch_field(offsetof(MenuResolutionEntry, m_format));
			patch_field(offsetof(MenuResolutionEntry, m_backBufferFormat));
			patch_field(offsetof(MenuResolutionEntry, field_54));
			patch_field(offsetof(MenuResolutionEntry, field_58));
			patch_field(offsetof(MenuResolutionEntry, field_5C));
			patch_field(offsetof(MenuResolutionEntry, field_60));
			patch_field(offsetof(MenuResolutionEntry, field_64));

			// GetMenuResolutionEntry
			placesToPatch.emplace_back(get_pattern("8D 04 91 8D 04 C5 ? ? ? ? C2 08 00", 3 + 3), 0);
		}
		catch (const hook::txn_exception&)
		{
			ResolutionsList::placesToPatch.clear();
		}
		ResolutionsList::placesToPatch.shrink_to_fit();
		
		// Allow all aspect ratios
		Patch<uint8_t>(check_aspect_ratio, 0xEB);

		// Set up for runtime patching of the hardcoded 128 resolutions list if there is a need
		// Not likely to be used anytime soon but it'll act as a failsafe just in case
		InterceptCall(get_display_mode_count, orgGetDisplayModeCount, GetDisplayModeCount_RelocateArray);
	}
	TXN_CATCH();


	// Do not crash if starting in fullscreen with an invalid resolution, instead try to find the closest matching resolution
	// Requires: CoreD3D
	if (HasCored3d && HasGraphics) try
	{
		using namespace FindClosestDisplayMode;	

		auto create_d3d_device = get_pattern("E8 ? ? ? ? 3B C3 0F 85 ? ? ? ? E8 ? ? ? ? A1 ? ? ? ? 8B 08");
		InterceptCall(create_d3d_device, orgCreateD3DDevice, FindClosestDisplayMode_CreateD3DDevice);
	}
	TXN_CATCH();


	// Default to desktop resolution
	try
	{
		DEVMODEW displaySettings;
		displaySettings.dmSize = sizeof(displaySettings);
		displaySettings.dmDriverExtra = 0;
		if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &displaySettings) != FALSE)
		{
			gResolutionWidthPixels = displaySettings.dmPelsWidth;
			gResolutionHeightPixels = displaySettings.dmPelsHeight;

			auto set_defaults = pattern("C7 44 24 ? 80 02 00 00 C7 44 24 ? E0 01 00 00 89 44 24 28").get_one();
			void* widths_to_patch[] = {
				get_pattern("68 80 02 00 00 68 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 50 E8", 1),
				get_pattern("68 80 02 00 00 68 ? ? ? ? 68 ? ? ? ? 89 44 24", 1),
				set_defaults.get<void>(4),
				get_pattern("68 80 02 00 00 68 ? ? ? ? 68 ? ? ? ? 8B E8", 1),
			};

			void* heights_to_patch[] = {
				get_pattern("68 E0 01 00 00 68 ? ? ? ? 68 ? ? ? ? 8B F8 E8 ? ? ? ? 50 E8", 1),
				get_pattern("68 E0 01 00 00 68 ? ? ? ? 68 ? ? ? ? 89 44 24", 1),
				set_defaults.get<void>(8 + 4),
				get_pattern("68 E0 01 00 00 68 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 6A 00", 1),
			};

			for (void* addr : widths_to_patch)
			{
				Patch<uint32_t>(addr, displaySettings.dmPelsWidth);
			}
			for (void* addr : heights_to_patch)
			{
				Patch<uint32_t>(addr, displaySettings.dmPelsHeight);
			}
		}
	}
	TXN_CATCH();


	// Fixed half pixel issues, and added line thickness
	// Requires patches: Graphics (for resolution), Viewport (for line thickness), Core D3D (for rectangle MSAA)
	if (HasGraphics && HasViewport && HasCored3d) try
	{
		using namespace HalfPixel;

		auto Blitter2D_Rect2D_G = pattern("76 39 8B 7C 24 3C").get_one();
		auto Blitter2D_Rect2D_GT = reinterpret_cast<intptr_t>(ReadCallFrom(get_pattern("E8 ? ? ? ? A1 ? ? ? ? 45 83 C3 40")));
		auto Blitter2D_Quad2D_G = reinterpret_cast<intptr_t>(ReadCallFrom(get_pattern("E8 ? ? ? ? 8B 4C 24 1C 8B 44 24 14")));
		auto Blitter2D_Quad2D_GT = pattern("2B EE 6B F6 54 55 03 F7 56 C7 05").get_one();
		auto Blitter2D_Line2D_G = pattern("8B 7C 24 28 1B C0").get_one();
		auto Blitter3D_Line3D_G = pattern("8B 75 0C 8B C2").get_one();
		auto matrices = pattern("BF ? ? ? ? F3 AB C7 05").get_one();

		Core_Blitter2D_Tri2D_G = reinterpret_cast<decltype(Core_Blitter2D_Tri2D_G)>(get_pattern("3B F2 57 76 3E", -0x30));
		Core_Blitter3D_Tri3D_G = reinterpret_cast<decltype(Core_Blitter3D_Tri3D_G)>(get_pattern("57 8B 7D 0C 8B C1", -0xD));

		pViewMatrix = *matrices.get<D3DMATRIX*>(-0xC + 1);
		pProjectionMatrix = *matrices.get<D3DMATRIX*>(1);

		dword_936C0C = *Blitter2D_Rect2D_G.get<void**>(-0x50 + 4);
		Core_Blitter2D_Rect2D_G_JumpBack = Blitter2D_Rect2D_G.get<void>(-0x50 + 8);
		InjectHook(Blitter2D_Rect2D_G.get<void>(-0x50), Core_Blitter2D_Rect2D_G_HalfPixel, PATCH_JUMP);

		Core_Blitter2D_Rect2D_GT_JumpBack = reinterpret_cast<void*>(Blitter2D_Rect2D_GT + 8);
		InjectHook(Blitter2D_Rect2D_GT, Core_Blitter2D_Rect2D_GT_HalfPixel, PATCH_JUMP);

		Core_Blitter2D_Quad2D_G_JumpBack = reinterpret_cast<void*>(Blitter2D_Quad2D_G + 8);
		InjectHook(Blitter2D_Quad2D_G, Core_Blitter2D_Quad2D_G_HalfPixel, PATCH_JUMP);

		Core_Blitter2D_Quad2D_GT_JumpBack = Blitter2D_Quad2D_GT.get<void>(-0x68 + 8);
		InjectHook(Blitter2D_Quad2D_GT.get<void>(-0x68), Core_Blitter2D_Quad2D_GT_HalfPixel, PATCH_JUMP);

		Core_Blitter2D_Line2D_G_JumpBack = Blitter2D_Line2D_G.get<void>(-0x17 + 9);
		InjectHook(Blitter2D_Line2D_G.get<void>(-0x17), Core_Blitter2D_Line2D_G_HalfPixelAndThickness, PATCH_JUMP);
		// Do not recurse into the custom function to avoid super-thick lines
		InjectHook(Blitter2D_Line2D_G.get<void>(0x2C), Core_Blitter2D_Line2D_G_Original);
		InjectHook(Blitter2D_Line2D_G.get<void>(0x44), Core_Blitter2D_Line2D_G_Original);

		Core_Blitter3D_Line3D_G_JumpBack = Blitter3D_Line3D_G.get<void>(-0xD + 6);
		InjectHook(Blitter3D_Line3D_G.get<void>(-0xD), Core_Blitter3D_Line3D_G_LineThickness, PATCH_JUMP);
		// Do not recurse into the custom function
		InjectHook(Blitter3D_Line3D_G.get<void>(0x34), Core_Blitter3D_Line3D_G_Original);
		InjectHook(Blitter3D_Line3D_G.get<void>(0x52), Core_Blitter3D_Line3D_G_Original);

	}
	TXN_CATCH();


	// Scissor-based digital tacho
	if (HasCored3d && HasGraphics && HasViewport && HasUIPositions) try
	{
		using namespace ScissorTacho;

		auto blit_texture = get_pattern("50 6A 00 6A 00 52 E8", 6);
		auto ftol = get_pattern("D9 44 24 10 E8 ? ? ? ? 8B 0D ? ? ? ? 6A FF 8B 51 0C", 4);

		InterceptCall(blit_texture, orgHandyFunction_BlitTexture, HandyFunction_BlitTexture_Scissor);
		InjectHook(ftol, ftol_fake);
	}
	TXN_CATCH();


	// Better line box drawing (without gaps and overlapping lines)
	if (HasBlitter2D && HasGraphics) try
	{
		using namespace BetterBoxDrawing;

		auto display_selection_box = get_pattern("81 E1 FF 00 00 00 99", -0xC);
		int32_t* car_setup_selection_box_posy = get_pattern<int32_t>("B9 ? ? ? ? 89 4C 24 1C", 1);
		int8_t* car_setup_selection_box_height = get_pattern<int8_t>("6A 08 6A 09", 1);

		auto osd_countdown_draw = pattern("D8 C9 D9 5C 24 3C DD D8 E8").count(4);
		auto osd_startlights_draw1 = pattern("D8 C9 D9 5C 24 38 DD D8 E8 ? ? ? ? E8 ? ? ? ? 89 44 24 10").count(2);
		void* osd_startlights_draw2[] = {
			get_pattern("6A 01 50 E8 ? ? ? ? E8 ? ? ? ? 89 44 24 10", 3),
			get_pattern("D9 5C 24 38 DD D8 E8 ? ? ? ? 5F 5E 5B", 6),
		};

		InjectHook(display_selection_box, DisplaySelectionBox, PATCH_JUMP);

		Patch<int32_t>(car_setup_selection_box_posy, 347 + 3);
		Patch<int8_t>(car_setup_selection_box_height, 9);

		InjectHook(osd_countdown_draw.get(0).get<void>(8), Blitter2D_Line2D_G_FirstLine);
		InjectHook(osd_startlights_draw1.get(0).get<void>(8), Blitter2D_Line2D_G_FirstLine);
		InjectHook(osd_countdown_draw.get(1).get<void>(8), Blitter2D_Line2D_G_SecondLine);
		InjectHook(osd_startlights_draw1.get(1).get<void>(8), Blitter2D_Line2D_G_SecondLine);
		InjectHook(osd_countdown_draw.get(2).get<void>(8), Blitter2D_Line2D_G_NOP);
		InjectHook(osd_countdown_draw.get(3).get<void>(8), Blitter2D_Line2D_G_NOP);
		for (void* addr : osd_startlights_draw2)
		{
			InjectHook(addr, Blitter2D_Line2D_G_NOP);
		}

		if (HasKeyboard)
		{
			InjectHook(::Keyboard_DrawTextEntryBox, BetterBoxDrawing::Keyboard_DrawTextEntryBox, PATCH_JUMP);
		}

		// Fixed HandyFunction_DrawClipped2DBox requires HandyFunction_Clip2DRect
		if (HasHandyFunction) try
		{
			auto draw_clipped_2d_box = get_pattern("53 55 56 33 F6 3B C6 57 0F 84", -0xA);

			InjectHook(draw_clipped_2d_box, BetterBoxDrawing::HandyFunction_DrawClipped2DBox, PATCH_JUMP);
		}
		TXN_CATCH();
	}
	TXN_CATCH();


	// Better widescreen support
	// Requires: Graphics
	if (HasGraphics && HasViewport) try
	{
		// Viewports
		try
		{
			auto set_viewport = ReadCallFrom(get_pattern("E8 ? ? ? ? 8B 4C 24 0C 8B 56 60"));
			auto set_aspect_ratios = get_pattern("83 EC 64 56 E8");

			auto recalc_fov = pattern("D8 0D ? ? ? ? DA 74 24 30 ").get_one();

			InjectHook(set_viewport, Viewport_SetDimensions, PATCH_JUMP);
			InjectHook(set_aspect_ratios, Graphics_Viewports_SetAspectRatios, PATCH_JUMP);

			// Change the horizontal FOV instead of vertical when refreshing viewports
			// fidiv -> fidivr and m_vertFov -> m_horFov
			static const float f4By3 = 3.0f/4.0f;
			Patch(recalc_fov.get<void>(2), &f4By3);
			Patch<uint8_t>(recalc_fov.get<void>(6+1), 0x7C);
			Patch<uint8_t>(recalc_fov.get<void>(10+2), 0x1C);

			// Pin viewports into constant aspect ratio
			try
			{
				using namespace ConstantViewports;

				std::array<void*, 3> viewports_constant_aspect_ratio =
				{
					get_pattern("E8 ? ? ? ? 8B 0D ? ? ? ? 8D 94 24"),
					get_pattern("E8 ? ? ? ? 6A 02 6A 01 6A 01"),
					get_pattern("E8 ? ? ? ? 6A 02 6A 01 6A 02"),
				};

				HookEach(viewports_constant_aspect_ratio, InterceptCall);
			}
			TXN_CATCH();
		}
		TXN_CATCH();

		// UI
		if (HasBlitter2D && HasCMR3Font && HasHandyFunction && HasKeyboard) try
		{
			using namespace Graphics::Patches;

			extern void (*orgSetMovieDirectory)(const char* path);

			std::array<void*, 2> graphics_change_recalculate_ui = {
				get_pattern("E8 ? ? ? ? 8B 54 24 24 89 5C 24 18"),
				get_pattern("E8 ? ? ? ? 8B 15 ? ? ? ? A1 ? ? ? ? 8B 0D"),
			};

			auto osd_data = pattern("03 C6 8D 0C 85 ? ? ? ? 8D 04 F5 00 00 00 00").get_one();

			void* osd_element_init_center[] = {
				get_pattern("52 50 E8 ? ? ? ? A1 ? ? ? ? C7 86", 2),
				get_pattern("E8 ? ? ? ? 68 ? ? ? ? C7 05 ? ? ? ? ? ? ? ? E8 ? ? ? ? 5E"),
			};
			
			void* osd_element_init_right[] = {
				get_pattern("E8 ? ? ? ? 33 C0 6A 01"),
				get_pattern("E8 ? ? ? ? 8B 0D ? ? ? ? 33 C0 81 E1"),
				get_pattern("68 ? ? ? ? E8 ? ? ? ? 5F 5E 5D C7 05", 5),
			};

			void* solid_background_full_width[] = {
				// Stage loading tiles
				get_pattern("E8 ? ? ? ? 46 83 C7 04 83 FE 05"),
			};

			// Constants to change
			auto patch_field = [](std::string_view str, ptrdiff_t offset)
			{
				pattern(str).for_each_result([offset](pattern_match match)
				{
					int32_t* const addr = match.get<int32_t>(offset);
					const int32_t val = *addr;
					UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, addr, val);
				});
			};

			auto patch_field_center = [](std::string_view str, ptrdiff_t offset)
			{
				pattern(str).for_each_result([offset](pattern_match match)
				{
					int32_t* const addr = match.get<int32_t>(offset);
					const int32_t val = *addr;
					UI_CenteredElements.emplace_back(std::in_place_type<Int32Patch>, addr, val);
				});
			};

			float* resolutionWidthMult1 = *get_pattern<float*>("DF 6C 24 18 D8 0D ? ? ? ? D9 5C 24 38", 4+2);
			if (mainModuleInstance == GetModuleHandleFromAddress(resolutionWidthMult1))
			{
				UI_resolutionWidthMult[0] = resolutionWidthMult1;
			}
			float* resolutionWidthMult2 = *get_pattern<float*>("DF 6C 24 08 D8 0D ? ? ? ? D9 5C 24 14", 4+2);
			if (mainModuleInstance == GetModuleHandleFromAddress(resolutionWidthMult2))
			{
				UI_resolutionWidthMult[1] = resolutionWidthMult2;
			}
			UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, *get_pattern<float*>("D8 3D ? ? ? ? D9 5C 24 18", 2), 640.0f);

			UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("B9 ? ? ? ? D9 5C 24 0C", 1), 640);
			UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("B9 ? ? ? ? 8D 3C B6", 1), 640);
			UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, *get_pattern<float*>("D8 E2 D8 0D ? ? ? ? D8 0D", 2+2), 590.0f);

			UI_CenteredElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("B8 ? ? ? ? 6A 01 6A 00 68", 1), 292);
			UI_CoutdownPosXVertical[0] = get_pattern<int32_t>("B8 ? ? ? ? B9 ? ? ? ? EB 1F E8", 1);
			UI_CoutdownPosXVertical[1] = get_pattern<int32_t>("76 0C B8 ? ? ? ? B9", 2+1);

			UI_MenuBarTextDrawLimit = get_pattern<int32_t>("C7 44 24 2C 01 00 00 00 81 FD", 8+2);

			UI_TachoInitialised = *get_pattern<int32_t*>("89 74 24 24 A1", 4+1);

			patch_field("05 89 01 00 00", 1); // add eax, 393
			patch_field("81 C5 89 01 00 00", 2); // add ebp, 393
			patch_field("81 C3 89 01 00 00", 2); // add ebx, 393
			patch_field("81 C7 89 01 00 00", 2); // add edi, 393
			patch_field("81 C6 89 01 00 00", 2); // add esi, 393

			// push 393
			patch_field("68 89 01 00 00 68 ? ? ? ? 6A 00", 1);
			UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("6A 09 51 68 89 01 00 00", 3 + 1), 393);

			// Menu arrows
			UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, *get_pattern<float*>("D8 0D ? ? ? ? F3 A5 D9 5C 24 2C", 2), 376.0f);
			pattern("C7 44 24 ? 00 00 BC 43").count(2).for_each_result([](pattern_match match)
			{
				UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, match.get<float>(4), 376.0f);
			});
			UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, get_pattern<float>("C7 44 24 ? 00 80 BA 43", 4), 376.0f - 3.0f);
			UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, get_pattern<float>("C7 84 24 ? ? ? ? 00 80 BA 43", 7), 376.0f - 3.0f);
			UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, get_pattern<float>("C7 44 24 ? 00 00 BE 43", 4), 376.0f + 3.0f);
			UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, get_pattern<float>("C7 84 24 ? ? ? ? 00 00 BE 43", 7), 376.0f + 3.0f);

			// Controls screen menu
			UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, get_pattern<float>("68 00 00 7F 43 52 56", 1), 255.0f);
			patch_field("81 C6 FF 00 00 00", 2); // add esi, 255
			patch_field("81 C3 FF 00 00 00", 2); // add ebx, 255
			UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("56 68 FF 00 00 00 68", 2), 255);

			// Championship standings pre-race
			void* race_standings[] = {
				get_pattern("E8 ? ? ? ? 6A 00 6A 00 6A 00 6A 00 6A FF E8 ? ? ? ? 33 ED"),
				get_pattern("E8 ? ? ? ? 55 E8 ? ? ? ? 50 E8 ? ? ? ? 8D 8C 24"),
				get_pattern("E8 ? ? ? ? 6A 0C 8D 54 24 38"),
				get_pattern("52 6A 00 E8 ? ? ? ? 55", 3),
				get_pattern("68 ? ? ? ? 52 6A 00 E8 ? ? ? ? 6A 00", 8),
			};
			
			// Championship standings pre-championship + unknown + Special Stage versus
			void* champ_standings1[] = {
				get_pattern("E8 ? ? ? ? 55 E8 ? ? ? ? 50 E8 ? ? ? ? 8D 4C 24 64"),
				get_pattern("51 6A 00 E8 ? ? ? ? 55 E8 ? ? ? ? 50", 3),
				get_pattern("6A 00 E8 ? ? ? ? 55 E8 ? ? ? ? 50 E8 ? ? ? ? 8D 4C 24 60", 2),
				get_pattern("50 6A 00 E8 ? ? ? ? 55", 3),
				get_pattern("E8 ? ? ? ? 6A 00 6A 00 6A 00 6A 00 6A FF E8 ? ? ? ? 5E 83 C4 08"),

				// Special Stage versus
				get_pattern("E8 ? ? ? ? E8 ? ? ? ? 50 E8 ? ? ? ? 85 C0 A1"),
				get_pattern("E8 ? ? ? ? A1 ? ? ? ? 83 F8 04"),
				get_pattern("E8 ? ? ? ? E8 ? ? ? ? 50 E8 ? ? ? ? 85 C0 75 42")
			};
			auto champ_standings2 = pattern("68 EA 01 00 00 50 6A 00 E8").count(2);

			void* champ_standings_redbar[] = {
				get_pattern("E8 ? ? ? ? EB 05 BB"),
				get_pattern("68 ? ? ? ? E8 ? ? ? ? EB 08", 5),

				// Special Stage versus
				get_pattern("E8 ? ? ? ? A1 ? ? ? ? 83 F8 03 75 0A"),
				get_pattern("68 ? ? ? ? E8 ? ? ? ? 6A 00 6A 00 6A 00", 5),
			};

			pattern("BA 80 02 00 00 2B D0").count(2).for_each_result([](pattern_match match)
			{
				UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, match.get<int32_t>(1), 640);
			});

			// Championship loading screen
			void* new_championship_loading_screen_text[] = {
				get_pattern("6A 0C E8 ? ? ? ? EB 04", 2),
				get_pattern("E8 ? ? ? ? 8B 44 24 2C 8B 4C 24 18"),
			};

			void* new_championship_loading_screen_rectangles[] = {
				get_pattern("E8 ? ? ? ? EB 02 DD D8 D9 44 24 14 D8 1D"),
				get_pattern("E8 ? ? ? ? EB 02 DD D8 D9 44 24 14 D8 1C AD"),
			};

			void* new_championship_loading_screen_lines[] = {
				get_pattern("DD D8 E8 ? ? ? ? D9 44 24 14", 2),
				get_pattern("E8 ? ? ? ? B8 ? ? ? ? 33 ED"),
				get_pattern("DD D8 E8 ? ? ? ? D9 44 24 20", 2),
			};

			// Stage loading background tiles
			UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, get_pattern<float>("C7 44 24 ? ? ? ? ? 89 44 24 3C 89 44 24 38 D8 0D", 4), 640.0f);
			UI_RightAlignElements.emplace_back(std::in_place_type<FloatPatch>, get_pattern<float>("DF 6C 24 78 C7 44 24", 4+4), 640.0f);

			// CMR3 logo in menus
			UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("68 ? ? ? ? 6A 40 6A 40", 1), 519);

			// CMR3 logo in the engagement screen
			patch_field_center("68 C0 00 00 00 68", 1);
			
			auto post_race_certina_logos1 = pattern("E8 ? ? ? ? 6A 15 E8 ? ? ? ? 50").count(5);
			auto post_race_certina_logos2 = pattern("E8 ? ? ? ? 68 51 02 00 00").count(2);
			auto post_race_certina_logos3 = pattern("E8 ? ? ? ? 68 46 02 00 00 E8").count(2);
			auto post_race_certina_logos4 = pattern("E8 ? ? ? ? 68 DA 00 00 00 E8").count(1);
			auto post_race_flags = pattern("E8 ? ? ? ? 83 ? ? 8B 54 24 ? 42").count(2);
			patch_field("68 34 02 00 00", 1); // push 564

			std::vector<void*> centered_blit_texts, right_blit_texts;
			
			// Post-race texts
			pattern("68 40 01 00 00 68 ? ? ? ? E8 ? ? ? ? 50 6A 00 E8 ? ? ? ? 5F 5E").count_hint(6).for_each_result([&](pattern_match match)
				{ // 6 in EFIGS/Polish, 4 in Czech
					centered_blit_texts.emplace_back(match.get<void>(18));
				});
			pattern("68 40 01 00 00 68 ? ? ? ? E8 ? ? ? ? 50 53 E8 ? ? ? ? 5D 5B").count_hint(2).for_each_result([&](pattern_match match)
				{ // 0 in EFIGS/Polish, 2 in Czech
					centered_blit_texts.emplace_back(match.get<void>(17));
				});
			pattern("68 40 01 00 00 68 ? ? ? ? E8 ? ? ? ? 50 6A 00 E8 ? ? ? ? 5E 5D").count_hint(1).for_each_result([&](pattern_match match)
				{ // 1 in EFIGS/Polish, 0 in Czech
					centered_blit_texts.emplace_back(match.get<void>(18));
				});
			pattern("68 40 01 00 00 68 ? ? ? ? E8 ? ? ? ? 50 55 E8 ? ? ? ? 5E 5D").count_hint(1).for_each_result([&](pattern_match match)
				{ // 0 in EFIGS/Polish, 1 in Czech
					centered_blit_texts.emplace_back(match.get<void>(17));
				});
	

			pattern("6A 00 E8 ? ? ? ? ? 83 ? 06").count(2).for_each_result([&](pattern_match match)
				{
					right_blit_texts.emplace_back(match.get<void>(2));
				});
			pattern("68 ? ? ? ? 68 ? ? ? ? 6A 0C E8 ? ? ? ? 8B 74 24 ? 8B 7C 24").count_hint(2).for_each_result([&](pattern_match match)
				{ // 2 in Polish/EFIGS, 0 in Czech
					right_blit_texts.emplace_back(match.get<void>(12));
				});
			pattern("68 ? ? ? ? 68 ? ? ? ? 6A 0C E8 ? ? ? ? 8B 44 24 ? 8B 7C 24").count_hint(2).for_each_result([&](pattern_match match)
				{ // 0 in Polish/EFIGS, 2 in Czech
					right_blit_texts.emplace_back(match.get<void>(12));
				});
			pattern("68 17 02 00 00 68 ? ? ? ? 6A 0C E8").count(4).for_each_result([&](pattern_match match)
				{
					right_blit_texts.emplace_back(match.get<void>(12));
				});
			
			// Time trial texts need a range check
			pattern(reinterpret_cast<uintptr_t>(ReadCallFrom(get_pattern("E8 ? ? ? ? EB 0F 8B 54 24 08"))),
												get_pattern_uintptr("8B 4C 24 08 56 8B 41 10"),
												"E8 ? ? ? ? 46 83 FE").count(5).for_each_result([&](pattern_match match)
				{
					right_blit_texts.emplace_back(match.get<void>());
				});

			// Super Special Stage time trial
			auto sss_trial_begin = reinterpret_cast<uintptr_t>(ReadCallFrom(get_pattern("50 E8 ? ? ? ? 8B 0D ? ? ? ? 6A 12", 1)));
			auto sss_trial_end = sss_trial_begin + 0xB20; // Hardcode an approximate size of the function... yuck.
			right_blit_texts.emplace_back(pattern(sss_trial_begin, sss_trial_end, "E8 ? ? ? ? 46 83 FE 02 7C 97").get_first<void>());
			pattern(sss_trial_begin, sss_trial_end, "68 2D 01 00 00").for_each_result([&](pattern_match match)
				{
					UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, match.get<int32_t>(1), 301);
				});
			pattern(sss_trial_begin, sss_trial_end, "68 7B 01 00 00").for_each_result([&](pattern_match match)
				{
					UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, match.get<int32_t>(1), 379);
				});
			
			// Shakedown
			pattern("E8 ? ? ? ? 46 83 FE 03 7C 9A").count(1).for_each_result([&](pattern_match match)
				{
					right_blit_texts.emplace_back(match.get<void>());
				});
			pattern("68 7B 01 00 00 68 ? ? ? ? 55 E8").count(3).for_each_result([&](pattern_match match)
				{
					right_blit_texts.emplace_back(match.get<void>(11));
				});
			pattern("68 C9 01 00 00 68 ? ? ? ? 6A 0C E8").count(3).for_each_result([&](pattern_match match)
				{
					right_blit_texts.emplace_back(match.get<void>(12));
				});

			// Stages high scores
			auto stages_highscores_begin = get_pattern_uintptr("53 55 56 57 33 FF 89 7C 24 14");
			auto stages_highscores_end = get_pattern_uintptr("0F 82 ? ? ? ? 6A 00 6A 00 6A 00 6A 00");
			pattern(stages_highscores_begin, stages_highscores_end, "68 ? ? ? ? 57 E8").for_each_result([&](pattern_match match)
				{
					centered_blit_texts.emplace_back(match.get<void>(6));
				});
			pattern(stages_highscores_begin, stages_highscores_end, "6A 00 E8").for_each_result([&](pattern_match match)
				{
					centered_blit_texts.emplace_back(match.get<void>(2));
				});
			pattern(stages_highscores_begin, stages_highscores_end, "6A 0C E8").for_each_result([&](pattern_match match)
				{
					centered_blit_texts.emplace_back(match.get<void>(2));
				});

			// Championship high scores
			auto championship_highscores_begin = get_pattern_uintptr("53 55 56 57 E8 ? ? ? ? 25");
			auto championship_highscores_end = get_pattern_uintptr("81 FF ? ? ? ? 0F 8C ? ? ? ? 6A 00");
			pattern(championship_highscores_begin, championship_highscores_end, "6A 00 E8").for_each_result([&](pattern_match match)
				{
					centered_blit_texts.emplace_back(match.get<void>(2));
				});
			pattern(championship_highscores_begin, championship_highscores_end, "6A 0C E8").for_each_result([&](pattern_match match)
				{
					centered_blit_texts.emplace_back(match.get<void>(2));
				});

			// OSD keyboard
			auto osd_keyboard_draw_text_entry_box1 = pattern("E8 ? ? ? ? 50 6A 1F 68 ? ? ? ? 68 ? ? ? ? 68 ? ? ? ? E8").count(3);
			auto osd_keyboard_draw_text_entry_box2 = pattern("E8 ? ? ? ? 8B 46 20 45 81 C7 ? ? ? ? 3B E8").count(3);
			auto osd_keyboard_blit_text_centered1 = pattern("50 6A 00 E8 ? ? ? ? 8B 15 ? ? ? ? 56 53 52").count(2);
			auto osd_keyboard_blit_text_centered2 = pattern("50 6A 00 E8 ? ? ? ? A1 ? ? ? ? 56").count(1);
			auto osd_keyboard_blit_text_centered3 = pattern("E8 ? ? ? ? FF 44 24 ? E9").count(3);
			auto osd_keyboard_blit_text_centered4 = pattern("E8 ? ? ? ? A1 ? ? ? ? 8B 54 24 18 52 53 50").count(3);
			void* osd_keyboard_best_score_text = get_pattern("E8 ? ? ? ? 8B 7D 14 C1 EF 0A");
			void* osd_keyboard_access_code_text = get_pattern("6A 0C E8 ? ? ? ? 8B 6C 24 24", 2);

			// Engagement screen
			auto engagement_screen_press_return_text1 = pattern("68 ? ? ? ? 6A 0C E8 ? ? ? ? D9 44 24").count(10);

			// Telemetry screen
			void* telemetry_legend_boxes_centered = get_pattern("E8 ? ? ? ? 8B 15 ? ? ? ? 6A 22");
			void* telemetry_texts_centered[] = {
				get_pattern("E8 ? ? ? ? 81 C7 ? ? ? ? 83 C5 04"),
				get_pattern("83 C4 08 50 53 E8", 5),
				get_pattern("E8 ? ? ? ? 8B 44 24 1C 8B 54 24 28"),
			};
			auto telemetry_lines1 = pattern("D8 C9 D9 5C 24 ? DD D8 E8 ? ? ? ? 83 C5 20").count(3);
			auto telemetry_lines2 = pattern("D8 C9 D9 9C 24 ? ? ? ? DD D8 E8 ? ? ? ? 83 C5 20").count(3);

			// Secrets screen
			// The amount of texts to patch differs between executables, so a range check is needed
			auto secrets_begin = pattern("C7 05 ? ? ? ? 80 02 00 00 D9 44 24 20").get_one();
			auto secrets_end = get_pattern_uintptr("89 44 24 1C 3B FA");
			pattern(secrets_begin.get_uintptr(), secrets_end, "68 40 01 00 00 68 ? ? ? ? E8 ? ? ? ? 50 6A ? E8").for_each_result([&](pattern_match match)
				{
					centered_blit_texts.emplace_back(match.get<void>(18));
				});
			pattern(secrets_begin.get_uintptr(), secrets_end, "68 40 01 00 00 68 ? ? ? ? 6A ? E8").for_each_result([&](pattern_match match)
				{
					centered_blit_texts.emplace_back(match.get<void>(12));
				});

			UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, secrets_begin.get<int32_t>(6), 640);
			UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("C7 05 ? ? ? ? ? ? ? ? E8 ? ? ? ? 68 ? ? ? ? 55", 6), 640); 
			UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("BF ? ? ? ? 2B F9", 1), 640);
			UI_RightAlignElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("BF ? ? ? ? 2B FA 2B F9", 1), 640);

			// Special case this one draw as we have to differentiate between a scroller text, and a normal text
			auto secrets_noautosave = pattern(secrets_begin.get_uintptr(), secrets_end, "E8 ? ? ? ? 8B 6C 24 1C 8B 45 08").get_first<void>();

			// "Original settings will be restored in X seconds" dialogs
			// + slot delete
			void* settings_reset_dialog_backgrounds[] = {
				get_pattern("DD D8 E8 ? ? ? ? E8 ? ? ? ? 68", 2),
				get_pattern("DD D8 E8 ? ? ? ? E8 ? ? ? ? 8B C8", 2),
			};

			void* settings_reset_dialog_texts[] = {
				get_pattern("E8 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 50 8D 4C 24 68"),
				get_pattern("E8 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 50 56"),
				get_pattern("51 6A 00 E8 ? ? ? ? 5F 5E 5B", 3),

				get_pattern("E8 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 50 8D 44 24 68"),
				get_pattern("E8 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 50 8D 54 24 68"),
				get_pattern("68 ? ? ? ? 50 6A 00 E8 ? ? ? ? 5F", 8),

				get_pattern("E8 ? ? ? ? 5E 8B 54 24 0C"),
			};

			patch_field_center("BA ? ? ? ? D1 F8", 1);

			// Controller calibration screen
			void* controller_calibrate_centered_texts[] = {
				get_pattern("C6 01 00 E8 ? ? ? ? A1", 3),
			};
			auto controller_calibrate_right_align_text = get_pattern("6A 00 E8 ? ? ? ? 8B 84 24 ? ? ? ? 85 C0 74 12 85 FF", 2);
			auto controller_calibrate_centered_box = get_pattern("E8 ? ? ? ? 8D 4C 24 2C 8B 44 24 24");

			auto controller_calibrate_centered_line1 = pattern("56 50 57 50 E8").count(2);
			auto controller_calibrate_centered_line2 = pattern("8B 44 24 1C 50 57 50 E8").count(4);

			// Splitscreen
			// Technically these are not centered, but same math applies
			UI_CenteredElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("83 FF 01 75 1F A1 ? ? ? ? C7 00", 12), 140);
			UI_CenteredElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("83 FF 03 75 3A 8B 0D ? ? ? ? B8 ? ? ? ? C7 01", 0x12), 140);
			UI_CenteredElements.emplace_back(std::in_place_type<Int32Patch>, get_pattern<int32_t>("8B 0D ? ? ? ? C7 01 ? ? ? ? 8B 15 ? ? ? ? C7 42", 6+2), 160);

			// Dirty disc error
			centered_blit_texts.emplace_back(get_pattern("E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? EB BB"));

			// Credits
			centered_blit_texts.emplace_back(get_pattern("6A 0C E8 ? ? ? ? 8B 2D", 2));
			centered_blit_texts.emplace_back(get_pattern("6A 0C E8 ? ? ? ? 8B 0D ? ? ? ? 8B 2D", 2));

			auto splitscreen_4th_viewport_rect2d = get_pattern("DD D8 E8 ? ? ? ? 8B 7C 24 30", 2);

			// Movie rendering
			auto movie_rect = pattern("C7 05 ? ? ? ? 00 00 00 BF C7 05 ? ? ? ? 00 00 00 BF").get_one();
			auto movie_name_setdir = get_pattern("E8 ? ? ? ? E8 ? ? ? ? 85 C0 A1 ? ? ? ? 0F 95 C3");
			UI_MovieX1 = movie_rect.get<float>(6);
			UI_MovieY1 = movie_rect.get<float>(10 + 6);
			UI_MovieX2 = movie_rect.get<float>(20 + 6);
			UI_MovieY2 = movie_rect.get<float>(30 + 6);

			orgOSDPositions = *osd_data.get<OSD_Data*>(2+3);
			orgOSDPositionsMulti = *osd_data.get<OSD_Data2*>(27+3);

			orgStartLightData = *get_pattern<Object_StartLight*>("8D 34 8D ? ? ? ? 89 74 24 0C", 3);

			// Assembly hook into HandyFunction_Draw2DBox and CMR3Font_SetViewport to stretch fullscreen draws automatically
			// Opt out by drawing 1px offscreen
			{
				using namespace SolidRectangleWidthHack;

				auto draw_solid_background = pattern("DB 44 24 5C 8B 44 24 6C").get_one();
				std::optional<pattern_match> set_string_extents;
				try
				{
					// Czech EXE matches on the original pattern too, but does it 1 byte too "early"
					set_string_extents.emplace(pattern("55 56 83 F8 FE 57").get_one());
				}
				catch (const hook::txn_exception&)
				{
					set_string_extents.emplace(pattern("56 83 F8 FE 57").get_one());
				}

				InjectHook(draw_solid_background.get<void>(-0x1A), HandyFunction_Draw2DBox_Hack, PATCH_JUMP);
				HandyFunction_Draw2DBox_JumpBack = draw_solid_background.get<void>(-0x1A + 5);

				InjectHook(set_string_extents->get<void>(-5), CMR3Font_SetViewport_Hack, PATCH_JUMP);
				CMR3Font_SetViewport_JumpBack = set_string_extents->get<void>(-5 + 5);
			}

			HookEach_RecalculateUI(graphics_change_recalculate_ui, InterceptCall);

			for (void* addr : osd_element_init_center)
			{
				InjectHook(addr, OSD_Element_Init_Center);
			}

			for (void* addr : osd_element_init_right)
			{
				InjectHook(addr, OSD_Element_Init_RightAlign);
			}

			for (void* addr : solid_background_full_width)
			{
				InjectHook(addr, HandyFunction_Draw2DBox_Stretch);
			}

			for (void* addr : race_standings)
			{
				InjectHook(addr, CMR3Font_BlitText_RightAlign);
			}
			for (void* addr : champ_standings1)
			{
				InjectHook(addr, CMR3Font_BlitText_RightAlign);
			}
			champ_standings2.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(8), CMR3Font_BlitText_RightAlign);
			});

			for (void* addr : champ_standings_redbar)
			{
				InjectHook(addr, HandyFunction_Draw2DBox_RightAlign);
			}

			for (void* addr : new_championship_loading_screen_text)
			{
				InjectHook(addr, CMR3Font_BlitText_Center);
			}
			for (void* addr : new_championship_loading_screen_rectangles)
			{
				InjectHook(addr, Core_Blitter2D_Rect2D_G_Center);
			}
			for (void* addr : new_championship_loading_screen_lines)
			{
				InjectHook(addr, Core_Blitter2D_Line2D_G_Center);
			}

			post_race_certina_logos1.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(), Core_Blitter2D_Rect2D_GT_RightAlign);
			});
			post_race_certina_logos2.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(), Core_Blitter2D_Rect2D_GT_RightAlign);
			});
			post_race_certina_logos3.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(), Core_Blitter2D_Rect2D_GT_RightAlign);
			});
			post_race_certina_logos4.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(), Core_Blitter2D_Rect2D_GT_RightAlign);
			});
			post_race_flags.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(), Core_Blitter2D_Rect2D_GT_RightAlign);
			});

			osd_keyboard_draw_text_entry_box1.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(0x17), Keyboard_DrawTextEntryBox_Center);
			});
			osd_keyboard_draw_text_entry_box2.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(), Keyboard_DrawTextEntryBox_Center);
			});
			osd_keyboard_blit_text_centered1.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(3), CMR3Font_BlitText_Center);
			});
			osd_keyboard_blit_text_centered2.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(3), CMR3Font_BlitText_Center);
			});
			osd_keyboard_blit_text_centered3.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(), CMR3Font_BlitText_Center);
			});
			osd_keyboard_blit_text_centered4.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(), CMR3Font_BlitText_Center);
			});
			InjectHook(osd_keyboard_best_score_text, CMR3Font_BlitText_Center);
			InjectHook(osd_keyboard_access_code_text, CMR3Font_BlitText_RightAlign);

			engagement_screen_press_return_text1.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(5 + 2), CMR3Font_BlitText_Center);
			});

			InjectHook(telemetry_legend_boxes_centered, HandyFunction_Draw2DBox_Center);
			for (void* addr : telemetry_texts_centered)
			{
				InjectHook(addr, CMR3Font_BlitText_Center);
			}
			telemetry_lines1.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(8), Core_Blitter2D_Line2D_G_Center);
			});
			telemetry_lines2.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(11), Core_Blitter2D_Line2D_G_Center);
			});

			InjectHook(secrets_noautosave, CMR3Font_BlitText_SecretsScreenNoAutoSave);

			for (void* addr : settings_reset_dialog_backgrounds)
			{
				InjectHook(addr, Core_Blitter2D_Rect2D_G_Center);
			}
			for (void* addr : settings_reset_dialog_texts)
			{
				InjectHook(addr, CMR3Font_BlitText_Center);
			}
			for (void* addr : controller_calibrate_centered_texts)
			{
				InjectHook(addr, CMR3Font_BlitText_Center);
			}
			InjectHook(controller_calibrate_right_align_text, CMR3Font_BlitText_CalibrateAxisName);
			InjectHook(controller_calibrate_centered_box, HandyFunction_Draw2DBox_Center);
			controller_calibrate_centered_line1.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(4), HandyFunction_Draw2DLineFromTo_Center);
			});
			controller_calibrate_centered_line2.for_each_result([](pattern_match match)
			{
				InjectHook(match.get<void>(7), HandyFunction_Draw2DLineFromTo_Center);
			});

			for (void* addr : centered_blit_texts)
			{
				InjectHook(addr, CMR3Font_BlitText_Center);
			}
			for (void* addr : right_blit_texts)
			{
				InjectHook(addr, CMR3Font_BlitText_RightAlign);
			}

			InjectHook(splitscreen_4th_viewport_rect2d, Core_Blitter2D_Rect2D_GT_CenterHalf);
			
			ReadCall(movie_name_setdir, orgSetMovieDirectory);
			InjectHook(movie_name_setdir, SetMovieDirectory_SetDimensions);

			OSD_Main_SetUpStructsForWidescreen();

			UIFullyPatched = true;
		}
		catch (const hook::txn_exception&)
		{
			Graphics::Patches::UI_CenteredElements.clear();
			Graphics::Patches::UI_RightAlignElements.clear();
		}
		Graphics::Patches::UI_CenteredElements.shrink_to_fit();
		Graphics::Patches::UI_RightAlignElements.shrink_to_fit();
	}
	TXN_CATCH();


	// Fixed "Player X has retired from race" drawing with incorrectly scaled coordinates
	if (HasCMR3Font && HasGraphics) try
	{
		auto retired_text = get_pattern("E8 ? ? ? ? 6A 00 53 E8 ? ? ? ? 5E");
		InjectHook(retired_text, CMR3Font_BlitText_RetiredFromRace);
	}
	TXN_CATCH();


	// Fixed split-screen time trials
	// Happens in the Czech EXE only, so these patterns are meant to fail with other versions
	try
	{
		using namespace CzechResultsScreen;
		
		// Special Stage Time Trial
		auto draw_clipped_box_save_y = get_pattern("E8 ? ? ? ? 3B 6C 24 ? 7D ? 8D 55 01");

		// Keep the ordering as-is!
		std::array<void*, 6> blit_texts_to_patch = {
			// Time Trial
			get_pattern("8B 4C 24 ? 83 C4 0C 6A 0C 53 51 6A ? 68 ? ? ? ? 6A 0C E8", 0x14),

			// Super Special Stage Time Trial
			get_pattern("E8 ? ? ? ? 8B 0E 41"), // Special cased

			get_pattern("E8 ? ? ? ? 8B 16 6A FF"),
			get_pattern("6A 00 E8 ? ? ? ? E8 ? ? ? ? 85 C0 74 1D", 2),
			get_pattern("E8 ? ? ? ? 8B 46 08 8B 4E 0C"),
			get_pattern("E8 ? ? ? ? 45 83 C6 18"),
		};

		HookEach(blit_texts_to_patch, InterceptCall);

		InterceptCall(draw_clipped_box_save_y, orgHandyFunction_DrawClipped2DBox, HandyFunction_DrawClipped2DBox_SaveY);
	}
	TXN_CATCH();


	// Fixed dial_002 cutting off by one pixel
	try
	{
		auto info_box_width = get_pattern("81 CE FF FF FF 00 56 6A 40 6A 40", 9+1);
		auto info_box_u2 = get_pattern("6A 40 6A 40 6A 00 6A 00 50", 2+1);

		Patch<int8_t>(info_box_width, 65);
		Patch<int8_t>(info_box_u2, 65);
	}
	TXN_CATCH();


	// Re-enabled Alt+F4
	try
	{
		using namespace QuitMessageFix;

		auto wndproc_messages_indirect_array = *get_pattern<uint8_t*>("8A 88 ? ? ? ? FF 24 8D ? ? ? ? 33 D2", 2);
		auto post_quit_message = get_pattern("6A 00 FF 15 ? ? ? ? E9", 2 + 2);
		gboExitProgram = *get_pattern<uint32_t*>("83 C4 04 39 2D", 3+2);

		const uint8_t def_proc = wndproc_messages_indirect_array[WM_CLOSE + 1 - 2];
		Patch<uint8_t>(&wndproc_messages_indirect_array[WM_CLOSE - 2], def_proc);

		Patch(post_quit_message, &pPostQuitMessage_AndRequestExit);
	}
	TXN_CATCH();


	// Fixed legend not fading on the telemetry screen
	try
	{
		using namespace TelemetryFadingLegend;

		auto draw_2d_box = pattern("6A ? 6A ? 8D 54 0A 19").get_one();

		originalHeight = *draw_2d_box.get<int8_t>(1);
		originalWidth = *draw_2d_box.get<int8_t>(3);

		// push dword ptr [esp+110h-D8h]
		Patch(draw_2d_box.get<void>(), { 0xFF, 0x74, 0x24, 0x38 });
		Patch<int8_t>(draw_2d_box.get<void>(10 + 3), 0x118 - 0xDC);

		InterceptCall(draw_2d_box.get<void>(18), orgHandyFunction_Draw2DBox, Draw2DBox_HackedAlpha);
	}
	TXN_CATCH();


	// Fixed the resolution change counter going into negatives
	try
	{
		using namespace CappedResolutionCountdown;

		auto countdown_sprintf = get_pattern("E8 ? ? ? ? 83 C4 14 8D 4C 24 64");
		InjectHook(countdown_sprintf, sprintf_clamp);
	}
	TXN_CATCH();


	// Fixed menu entries fading incorrectly
	try
	{
		auto on_submenu_enter = get_pattern("0F BF 46 18 39 44 24 20 75", 8);
		auto on_submenu_exit = get_pattern("39 54 24 20 74", 4);

		Patch<uint8_t>(on_submenu_enter, 0xEB);
		Nop(on_submenu_exit, 2);
	}
	TXN_CATCH();


	// Fixed an inconsistent Controls screen
	try
	{
		using namespace ConsistentControlsScreen;

		char* wrong_format_string = *get_pattern<char*>("68 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? 8B 4C 24 34", 1);
		auto uppercase_controller_name = get_pattern("E8 ? ? ? ? 8B 44 24 20 83 F8 04");

		auto get_language_sprintf_pattern = pattern("50 56 68 74 03 00 00 E8 ? ? ? ? 50").count(2);
		std::array<void*, 2> get_language_sprintf = {
			get_language_sprintf_pattern.get(0).get<void>(7),
			get_language_sprintf_pattern.get(1).get<void>(7),
		};

		// Patch the string directly, whatevz
		if (mainModuleInstance == GetModuleHandleFromAddress(wrong_format_string))
		{
			strcpy_s(wrong_format_string, 6, "%s: ");
		}

		InterceptCall(uppercase_controller_name, orgGetControllerName, GetControllerName_Uppercase);

		HookEach(get_language_sprintf, InterceptCall);
	}
	TXN_CATCH();


	// "Cut" character flicker for invalid codepoints - let it be ! now
	try
	{
		using namespace UnrandomizeUnknownCodepoints;

		auto rand_sequence1 = pattern("E8 ? ? ? ? 6A 00 88 44 24 20 E8 ? ? ? ? 6A 00 88 44 24 24 E8 ? ? ? ? 25").get_one();
		void* rand_sequence[] = {
			rand_sequence1.get<void>(),
			rand_sequence1.get<void>(0xB),
			rand_sequence1.get<void>(0x16),
			get_pattern("89 4C 24 18 E8 ? ? ? ? 8B 4D 00 33 D2", 4),
		};

		auto set_current_font = pattern("8B 44 24 24 8B 4C 24 34 50 51 E8").get_one();

		for (void* addr : rand_sequence)
		{
			InjectHook(addr, Random_RandSequence_Fixed);
		}

		InjectHook(set_current_font.get<void>(0xA), CMR3Font_SetFontForBlitChar_NOP);
		InjectHook(set_current_font.get<void>(0x34), CMR3Font_SetFontForBlitChar_NOP);
	}
	TXN_CATCH();


	// Pump messages in the file error callback
	try
	{
		using namespace FileErrorMessages;

		auto graphics_render = get_pattern("E8 ? ? ? ? E8 ? ? ? ? EB BB", 5);

		InterceptCall(graphics_render, orgGraphics_Render, Graphics_Render_PumpMessages);
	}
	TXN_CATCH();


	// Disable forced Z test in Quad2D_GT and Line2D_G functions (on demand)
	// to fix broken car shadow soften pass with MSAA enabled
	try
	{
		using namespace ConditionalZWrite;

		auto blitter2d_set = pattern("FF ? 30 6A 01 E8 ? ? ? ? 6A 01 8B F0").count(7);
		auto blitter2d_unset = pattern("56 89 2D ? ? ? ? E8 ? ? ? ? 57 E8").count(6); // All except for Line2D
		auto blitter2d_unset_line2d = get_pattern("FF 91 ? ? ? ? 56 E8 ? ? ? ? 53", 7);

		auto graphics_shadow_soften = get_pattern("6A 08 E8 ? ? ? ? 8B 4C 24 1C", 2);

		std::array<void*, 14> zbuffer_modes = {
			blitter2d_set.get(0).get<void>(5), blitter2d_set.get(1).get<void>(5), blitter2d_set.get(2).get<void>(5),
			blitter2d_set.get(3).get<void>(5), blitter2d_set.get(4).get<void>(5), blitter2d_set.get(5).get<void>(5),
			blitter2d_set.get(6).get<void>(5),

			blitter2d_unset.get(0).get<void>(7), blitter2d_unset.get(1).get<void>(7), blitter2d_unset.get(2).get<void>(7),
			blitter2d_unset.get(3).get<void>(7), blitter2d_unset.get(4).get<void>(7), blitter2d_unset.get(5).get<void>(7),

			blitter2d_unset_line2d
		};

		// Only patch the switch if we have registry
		if (HasRegistry) try
		{
			auto after_setup_texture_stages = get_pattern("DD D8 E8 ? ? ? ? 50 E8", 2);

			InterceptCall(after_setup_texture_stages, orgAfterSetupTextureStages, Graphics_CarMultitexture_AfterSetupTextureStages);
		}
		TXN_CATCH();

		HookEach_ConditionalSet(zbuffer_modes, InterceptCall);

		InterceptCall(graphics_shadow_soften, orgGraphics_Shadows_Soften, Graphics_Shadows_Soften_DisableDepth);
	}
	TXN_CATCH();


	// Fix water reflections...
	{
		using namespace FixedWaterReflections;

		// ...not reinitializing with Alt+Tab
		try
		{
			auto restore_all_device_objects = get_pattern("89 1D ? ? ? ? 89 1D ? ? ? ? E8 ? ? ? ? 5F", 12);
			auto countdown = *get_pattern<uint32_t*>("39 2D ? ? ? ? 74 10", 2);

			reinitCountdown = countdown;
			InterceptCall(restore_all_device_objects, orgSystem_RestoreAllDeviceObjects, System_RestoreAllDeviceObjects_SetCountdown);
		}
		TXN_CATCH();

		// ...getting a wrong render target format after a device reset
		// (might also affect other effects, as device reset makes them lose alpha)
		try
		{
			auto formats_to_set = pattern(get_pattern_uintptr("57 89 4C 24 1C 8B 56 54"), get_pattern_uintptr("89 7C 24 20 5F"), "8B ? 50").count(4);

			formats_to_set.for_each_result([](pattern_match match)
			{
				Patch<int8_t>(match.get<void>(2), 0x4C);
			});
		}
		TXN_CATCH();

		// ...breaking due to cache coherency issues; restore missing several render states
		// (might affect more effects, but water is the only known breakage)
		if (HasCored3d && HasRenderState) try
		{
			auto render_state_flush_restore = get_pattern("FF 91 ? ? ? ? E8 ? ? ? ? 33 F6", 6);

			InterceptCall(render_state_flush_restore, orgRenderState_Flush, RenderState_Flush_RestoreMissingStates);
		}
		TXN_CATCH();
	}


	// Make the game portable
	// Removes settings from registry and reliance on INSTALL_PATH
	if (HasRegistry) try
	{
		using namespace Registry;
		using namespace Patches;

		auto get_install_string_operator_new = get_pattern("E8 ? ? ? ? 8B 54 24 10 83 C4 04");
		auto get_install_string = get_pattern("E8 ? ? ? ? 50 E8 ? ? ? ? 68 ? ? ? ? 8B D8 68");
		auto get_registry_dword = get_pattern("83 EC 08 8B 4C 24 0C 8D 44 24 04");
		auto set_registry_dword = get_pattern("8D 54 24 0C 6A 04", -0x24);
		auto set_registry_char = get_pattern("8B 4C 24 04 8D 54 24 0C 6A 01", -0x20);

		ReadCall(get_install_string_operator_new, Patches::orgOperatorNew);
		InjectHook(get_install_string, GetInstallString_Portable);

		InjectHook(get_registry_dword, GetRegistryDword_Patch, PATCH_JUMP);
		InjectHook(set_registry_dword, SetRegistryDword_Patch, PATCH_JUMP);
		InjectHook(set_registry_char, SetRegistryChar_Patch, PATCH_JUMP);

		// This one is optional! Polish exe lacks it
		try
		{
			auto get_registry_char = get_pattern("85 C0 75 ? 8B 4C 24 14 56 8B 74 24 1C 8D 54 24 04 57", -0x1F);
			InjectHook(get_registry_char, GetRegistryChar_Patch, PATCH_JUMP);
		}
		TXN_CATCH();
	}
	TXN_CATCH();


	// Reflection map reflecting the sky
	// Registry is an optional dependency - without it, we can't restore "old" reflections via the INI file
	try
	{
		using namespace EnvMapWithSky;

		auto render_refmap_sfx_check = get_pattern("6A 02 E8 ? ? ? ? 85 C0 0F 85 ? ? ? ? 89 44 24 60", 2);

		InterceptCall(render_refmap_sfx_check, orgSfxEnable, SFX_Enable_Force);

		// Only patch the switch if we have registry
		if (HasRegistry) try
		{
			auto after_setup_texture_stages = get_pattern("DD D8 E8 ? ? ? ? 50 E8", 2);

			InterceptCall(after_setup_texture_stages, orgAfterSetupTextureStages, Graphics_CarMultitexture_AfterSetupTextureStages);
		}
		TXN_CATCH();
	}
	TXN_CATCH();


	// Remapped menu navigation from analog sticks to DPad
	if ((HasRegistry && Registry::GetRegistryDword(Registry::ADVANCED_SECTION_NAME, Registry::ANALOG_MENU_NAV_KEY_NAME).value_or(0) == 0) || !HasRegistry) try
	{
		auto axis_type_analog = pattern("83 F8 01 75 09 83 FD FF 75 10").get_one();

		// DPad is an axis type 8
		Patch<int8_t>(axis_type_analog.get<void>(2), 8);

		// Instead of taking the first axis type 1 and the first axis type 2, take the first and second axis type 8
		// jnz loc_4439A6 -> jnz loc_44399F
		Patch<int8_t>(axis_type_analog.get<void>(8 + 1), 9);
	}
	TXN_CATCH();


	// Additional Advanced Graphics options
	// Windowed Mode, VSync, Anisotropic Filtering, Refresh Rate
	// Requires patches: Registry (for saving/loading), Core D3D (for resizing windows), Graphics, RenderState (for AF)
	if (HasRegistry && HasCored3d && HasDestruct && HasGraphics && HasRenderState) try
	{
		using namespace NewAdvancedGraphicsOptions;

		// Both inner scopes patch this independently
		auto graphics_change_pattern = pattern("85 C0 74 0A 8D 4C 24 54").get_one();
		auto gamma_on_adapter_change = pattern("C7 83 ? ? ? ? ? ? ? ? EB 1B").get_one();

		// Try to decouple frontend options from the actual implementations
		bool HasPatches_AdvancedGraphics = false;
		try
		{
			auto nop_adjust_windowrect = get_pattern("FF 15 ? ? ? ? 6A 00", 2);
			auto find_window_ex = get_pattern("FF 15 ? ? ? ? 85 C0 74 0A", 2);
			auto create_class_and_window = get_pattern("83 EC 28 8B 44 24 38");
			auto graphics_initialise = get_pattern("E8 ? ? ? ? 8B 54 24 24 89 5C 24 18");

			auto set_sampler_state = reinterpret_cast<uintptr_t>(ReadCallFrom(get_pattern("E8 ? ? ? ? D9 44 24 0C D9 1C B5 ? ? ? ? D9 44 24 08")));
			auto render_game_common1_set_af = get_pattern("E8 ? ? ? ? 68 ? ? ? ? 6A 03 E8 ? ? ? ? E8 ? ? ? ? 8B 5C 24 30 85 C0 74 06 53 E8");
			auto get_filtering_method_for_3d = ReadCallFrom(get_pattern("E8 ? ? ? ? 8B 17 50"));
			auto set_mip_bias_and_anisotropic = get_pattern("E8 ? ? ? ? 8D 4C 24 20 51 57");

			std::array<void*, 2> graphics_change = {
				graphics_change_pattern.get<void>(-5),
				graphics_change_pattern.get<void>(9),
			};

			std::array<void*, 2> nop_presentinterval_changes = {
				get_pattern("74 12 8D 4C 24 04"),
				get_pattern("74 0E 8D 4C 24 0C"),
			};

			auto set_window_pos_adjust = pattern("50 FF 15 ? ? ? ? A1 ? ? ? ? 8B 0D ? ? ? ? 89 44 24 0C").get_one();
			auto disable_gamma_for_window = get_pattern("56 57 55 50 E8 ? ? ? ? B9 4C 00 00 00", 4);

			auto get_num_modes = get_pattern("55 E8 ? ? ? ? 33 C9 3B C1 89 44 24", 1);

			std::array<void*, 2> update_presentation_parameters = {
				get_pattern("FF 15 ? ? ? ? 68 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 3B C3 0F 85", 11),
				get_pattern("E8 ? ? ? ? 8B 15 ? ? ? ? A1 ? ? ? ? 8B 0D"),
			};

			auto get_system_metrics = get_pattern("8B 2D ? ? ? ? 6A 00", 2);

			ghInstance = *get_pattern<HINSTANCE*>("89 3D ? ? ? ? 89 44 24 14", 2);

			Patch(nop_adjust_windowrect, &pAdjustWindowRectEx_NOP);
			Patch(find_window_ex, &pFindWindowExA_IgnoreWindowName);
			InjectHook(create_class_and_window, CreateClassAndWindow, PATCH_JUMP);
			InterceptCall(graphics_initialise, orgGraphics_Initialise, Graphics_Initialise_NewOptions);

			HookEach_Graphics_Change(graphics_change, InterceptCall);

			Patch(set_window_pos_adjust.get<void>(-0x22 + 2), &pSetWindowLongA_NOP);
			Patch(set_window_pos_adjust.get<void>(1 + 2), &pSetWindowPos_Adjust);

			InterceptCall(disable_gamma_for_window, orgGraphics_GetAdapterCaps, Graphics_GetAdapterCaps_DisableGammaForWindow);

			InterceptCall(get_num_modes, orgGetNumModes, GetNumModes_PopulateRefreshRates);

			// NOP gamma changes in windowed mode so switching to windowed mode doesn't reset the option
			Nop(gamma_on_adapter_change.get<void>(0x12), 10);

			for (void* addr : nop_presentinterval_changes)
			{
				Patch<uint8_t>(addr, 0xEB);
			};

			guSSChanges = *reinterpret_cast<int**>(set_sampler_state + 2);
			RenderState_SetSamplerState_JumpBack = reinterpret_cast<void*>(set_sampler_state + 6);
			InjectHook(set_sampler_state, RenderState_SetSamplerState_Fallback, PATCH_JUMP);

			InterceptCall(render_game_common1_set_af, orgMarkProfiler, MarkProfiler_SetAF);
			InjectHook(get_filtering_method_for_3d, GetAnisotropicFilter, PATCH_JUMP);
			InterceptCall(set_mip_bias_and_anisotropic, orgSetMipBias, Texture_SetMipBiasAndAnisotropic);

			HookEach_UpdatePresentationParameters(update_presentation_parameters, InterceptCall);

			// Re-populate all displays
			Patch(get_system_metrics, &pGetSystemMetrics_Desktop);

			DesktopRects.clear();
			EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR, HDC, LPRECT lpRect, LPARAM) -> BOOL
				{
					DesktopRects.emplace_back(*lpRect);
					return TRUE;
				}, 0);

			HasPatches_AdvancedGraphics = true;
		}
		TXN_CATCH();


		// Only make frontend changes if all functionality has been successfully patched in
		if (HasGlobals && HasGraphics && HasCMR3FE && HasCMR3Font && HasMenuHook && HasLanguageHook && HasPatches_AdvancedGraphics) try
		{
			auto graphics_advanced_enter = pattern("8B 46 24 A3 ? ? ? ? 8B 4E 4C").get_one();
			auto graphics_advanced_select = get_pattern("50 E8 ? ? ? ? 8B 0D ? ? ? ? 8B 74 24 58", -0xD);

			auto advanced_graphics_load_settings = get_pattern("83 EC 58 53 55 56 57 6A 00 68");
			auto advanced_graphics_save_settings = ReadCallFrom(get_pattern("E8 ? ? ? ? 8B 44 24 04 8B 08"));

			auto set_graphics_from_preset = get_pattern("25 ? ? ? ? 33 C9 2B C1", -6);

			auto advanced_graphics_display_jump_table = pattern("89 7C 24 18 8B ? 83 F8 ? 0F 87").get_one();

			auto advanced_graphics_texture_quality_display_value = get_pattern("8B 86 ? ? ? ? 81 C7 ? ? ? ? 83 E8 00 74 0A", 2);

			std::vector<std::pair<void*, size_t>> menu_locals;

			// These are byte accesses in EFIGS/Polish EXEs, but dword accesses in the Czech EXE
			try
			{
				auto envmap = pattern("8A 86 ? ? ? ? 8B 8C 24").get_one();
				auto shadows = pattern("8A 8E ? ? ? ? 8B 94 24").get_one();

				menu_locals.emplace_back(envmap.get<void>(2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_visibilityAndName) + 3);
				menu_locals.emplace_back(envmap.get<void>(6 + 7 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_value));

				menu_locals.emplace_back(shadows.get<void>(2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_visibilityAndName) + 3);
				menu_locals.emplace_back(shadows.get<void>(6 + 7 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_value));

			}
			catch (const hook::txn_exception&)
			{
				auto envmap = pattern("8B 86 ? ? ? ? 8B 8C 24").get_one();
				auto shadows = pattern("8B 8E ? ? ? ? 8B 94 24").get_one();

				menu_locals.emplace_back(envmap.get<void>(2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_visibilityAndName));
				menu_locals.emplace_back(envmap.get<void>(6 + 7 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_value));

				menu_locals.emplace_back(shadows.get<void>(2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_visibilityAndName));
				menu_locals.emplace_back(shadows.get<void>(6 + 7 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_value));
			}

			auto advanced_graphics_draw_distance_display = pattern("8B 8E ? ? ? ? 8B F8 81 C7").get_one();
			auto advanced_graphics_gamma_display = pattern("8B 86 ? ? ? ? 81 C7 ? ? ? ? 40").get_one();
			auto advanced_graphics_fsaa_display_value = get_pattern("8B 96 ? ? ? ? 8B 40 4C", 2);

			auto advanced_graphics_texture_quality_display_arrow = get_pattern("52 6A 03 56", 2);
			auto advanced_graphics_draw_distance_display_arrow = get_pattern("50 6A 06 56", 2);
			auto advanced_graphics_gamma_display_arrow = get_pattern("52 6A 07 56", 2);
			auto advanced_graphics_fsaa_display_arrow = get_pattern("52 6A 08 56", 2);

			auto advanced_graphics_handle_hook = pattern("83 EC 50 8D 44 24 00 53 55 56 57 50 E8").get_one();

			PC_GraphicsAdvanced_Display_NewOptionsJumpBack = get_pattern("8B 44 24 1C 8B 4C 24 18 25 ? ? ? ? 6A 09");

			void* fsaa_globals_value[] = {
				get_pattern("A1 ? ? ? ? 89 7C 24 20", 1),
			};

			menu_locals.emplace_back(get_pattern("51 8B 7C 24 68 DB 87", 1 + 4 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_GAMMA].m_value));

			try
			{
				// EFIGS/Polish
				auto fsaa_on_adapter_change = pattern("8B 83 ? ? ? ? 7F 0A").get_one();
				auto envmap_on_adapter_change1 = get_pattern("89 15 ? ? ? ? 8D 84 24", -6 + 2);
				auto envmap_on_adapter_change2 = pattern("8B 83 ? ? ? ? 75 22").get_one();
				auto shadows_on_adapter_change = pattern("89 15 ? ? ? ? 85 84 24").get_one();

				menu_locals.emplace_back(fsaa_on_adapter_change.get<void>(-6 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_FSAA].m_entryDataInt));
				menu_locals.emplace_back(fsaa_on_adapter_change.get<void>(0 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_FSAA].m_visibilityAndName));
				menu_locals.emplace_back(fsaa_on_adapter_change.get<void>(8 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_FSAA].m_value));
				menu_locals.emplace_back(fsaa_on_adapter_change.get<void>(0x17 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_FSAA].m_visibilityAndName));

				menu_locals.emplace_back(envmap_on_adapter_change1, offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_value));

				menu_locals.emplace_back(envmap_on_adapter_change2.get<void>(0 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_visibilityAndName));
				menu_locals.emplace_back(envmap_on_adapter_change2.get<void>(0xF + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_value));
				menu_locals.emplace_back(envmap_on_adapter_change2.get<void>(0x19 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_visibilityAndName));
				menu_locals.emplace_back(envmap_on_adapter_change2.get<void>(0x37 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_value));
				menu_locals.emplace_back(envmap_on_adapter_change2.get<void>(0x37 + 6 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_visibilityAndName));

				menu_locals.emplace_back(shadows_on_adapter_change.get<void>(-6 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_value));
				menu_locals.emplace_back(shadows_on_adapter_change.get<void>(0xF + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_visibilityAndName));
				menu_locals.emplace_back(shadows_on_adapter_change.get<void>(0xF + 6 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_value));
				menu_locals.emplace_back(shadows_on_adapter_change.get<void>(0x2B + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_value));
				menu_locals.emplace_back(shadows_on_adapter_change.get<void>(0x31 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_visibilityAndName));
				menu_locals.emplace_back(shadows_on_adapter_change.get<void>(0x41 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_visibilityAndName));
			}
			catch (const hook::txn_exception&)
			{
				// Czech
				auto fsaa_on_adapter_change = pattern("8B 83 ? ? ? ? 7E 0D").get_one();
				auto envmap_on_adapter_change1 = get_pattern("89 15 ? ? ? ? 8D 44 24", -6 + 2);
				auto envmap_on_adapter_change2 = pattern("8B 83 ? ? ? ? 25 FF FF FF FE 5F").get_one();
				auto shadows_on_adapter_change = pattern("89 15 ? ? ? ? 85 44 24").get_one();
					
				menu_locals.emplace_back(fsaa_on_adapter_change.get<void>(-6 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_FSAA].m_entryDataInt));
				menu_locals.emplace_back(fsaa_on_adapter_change.get<void>(0 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_FSAA].m_visibilityAndName));
				menu_locals.emplace_back(fsaa_on_adapter_change.get<void>(0xD + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_FSAA].m_visibilityAndName));
				menu_locals.emplace_back(fsaa_on_adapter_change.get<void>(0x24 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_FSAA].m_visibilityAndName));
				menu_locals.emplace_back(fsaa_on_adapter_change.get<void>(0x1A + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_FSAA].m_value));

				menu_locals.emplace_back(envmap_on_adapter_change1, offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_value));

				menu_locals.emplace_back(envmap_on_adapter_change2.get<void>(0 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_visibilityAndName));
				menu_locals.emplace_back(envmap_on_adapter_change2.get<void>(0xD + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_visibilityAndName));
				menu_locals.emplace_back(envmap_on_adapter_change2.get<void>(0x1E + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_visibilityAndName));
				menu_locals.emplace_back(envmap_on_adapter_change2.get<void>(0x37 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_visibilityAndName));

				menu_locals.emplace_back(envmap_on_adapter_change2.get<void>(-6 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_value));
				menu_locals.emplace_back(envmap_on_adapter_change2.get<void>(0x31 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_ENVMAP].m_value));

				menu_locals.emplace_back(shadows_on_adapter_change.get<void>(-6 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_value));
				menu_locals.emplace_back(shadows_on_adapter_change.get<void>(0x12 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_value));
				menu_locals.emplace_back(shadows_on_adapter_change.get<void>(0x28 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_value));

				menu_locals.emplace_back(shadows_on_adapter_change.get<void>(0xC + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_visibilityAndName));
				menu_locals.emplace_back(shadows_on_adapter_change.get<void>(0x2E + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_visibilityAndName));
				menu_locals.emplace_back(shadows_on_adapter_change.get<void>(0x3E + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_visibilityAndName));
			}

			InjectHook(advanced_graphics_load_settings, PC_GraphicsAdvanced_LoadSettings, PATCH_JUMP);
			InjectHook(advanced_graphics_save_settings, PC_GraphicsAdvanced_SaveSettings, PATCH_JUMP);

			InjectHook(set_graphics_from_preset, PC_GraphicsAdvanced_SetGraphicsFromPresetQuality, PATCH_JUMP);

			menu_locals.emplace_back(graphics_advanced_enter.get<void>(0x22 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_SHADOWS].m_value));

			InjectHook(graphics_advanced_enter.get<void>(0x3B), PC_GraphicsAdvanced_Enter_NewOptions, PATCH_JUMP);
			InjectHook(graphics_advanced_select, PC_GraphicsAdvanced_Select_NewOptions, PATCH_JUMP);

			for (void* addr : fsaa_globals_value)
			{
				Patch(addr, &gmoFrontEndMenus[MenuID::GRAPHICS_ADVANCED].m_entries[EntryID::GRAPHICS_ADV_FSAA].m_value);
			}
			
			menu_locals.emplace_back(gamma_on_adapter_change.get<void>(0 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_GAMMA].m_entryDataInt));
			menu_locals.emplace_back(gamma_on_adapter_change.get<void>(0xC + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_GAMMA].m_visibilityAndName));
			// Gamma m_value change was NOP'd above
			//menu_locals.emplace_back(gamma_on_adapter_change.get<void>(0x12 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_GAMMA].m_value));
			menu_locals.emplace_back(gamma_on_adapter_change.get<void>(0x21 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_GAMMA].m_visibilityAndName));

			// Build and inject a new jump table
			Patch<uint8_t>(advanced_graphics_display_jump_table.get<void>(4 + 2 + 2), EntryID::GRAPHICS_ADV_NUM - 1);

			void** orgJumpTable = *advanced_graphics_display_jump_table.get<void**>(4 + 0xB + 3);
			static const void* advanced_graphics_display_new_jump_table[EntryID::GRAPHICS_ADV_NUM] = {
				orgJumpTable[0], orgJumpTable[1], orgJumpTable[2],
				&PC_GraphicsAdvanced_Display_CaseNewOptions, &PC_GraphicsAdvanced_Display_CaseNewOptions, &PC_GraphicsAdvanced_Display_CaseNewOptions,
				orgJumpTable[3], orgJumpTable[4], orgJumpTable[5], orgJumpTable[6], &PC_GraphicsAdvanced_Display_CaseNewOptions, orgJumpTable[7], orgJumpTable[8],
				orgJumpTable[9], orgJumpTable[10]
			};
			Patch(advanced_graphics_display_jump_table.get<void**>(4 + 0xB + 3), &advanced_graphics_display_new_jump_table);

			menu_locals.emplace_back(advanced_graphics_texture_quality_display_value, offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_TEXTUREQUALITY].m_value));
			Patch<int8_t>(advanced_graphics_texture_quality_display_arrow, EntryID::GRAPHICS_ADV_TEXTUREQUALITY);

			menu_locals.emplace_back(advanced_graphics_draw_distance_display.get<void>(2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_DRAWDISTANCE].m_value));
			menu_locals.emplace_back(advanced_graphics_draw_distance_display.get<void>(0x24 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_DRAWDISTANCE].m_value));
			Patch<int8_t>(advanced_graphics_draw_distance_display_arrow, EntryID::GRAPHICS_ADV_DRAWDISTANCE);

			menu_locals.emplace_back(advanced_graphics_gamma_display.get<void>(2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_GAMMA].m_value));
			menu_locals.emplace_back(advanced_graphics_gamma_display.get<void>(0x22 + 2), offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_GAMMA].m_value));
			Patch<int8_t>(advanced_graphics_gamma_display_arrow, EntryID::GRAPHICS_ADV_GAMMA);

			menu_locals.emplace_back(advanced_graphics_fsaa_display_value, offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_ADV_FSAA].m_value));
			Patch<int8_t>(advanced_graphics_fsaa_display_arrow, EntryID::GRAPHICS_ADV_FSAA);

			InterceptCall(graphics_change_pattern.get<void>(-5), orgGraphics_SetupRenderFromMenuOptions, Graphics_Change_SetupRenderFromMenuOptions);

			for (const auto& addr : menu_locals)
			{
				Patch<int32_t>(std::get<void*>(addr), std::get<size_t>(addr));
			}

			PC_GraphicsAdvanced_Handle_JumpBack = advanced_graphics_handle_hook.get<void>(7);
			InjectHook(advanced_graphics_handle_hook.get<void>(), PC_GraphicsAdvanced_Handle_NewOptions, PATCH_JUMP);

			Menus::Patches::ExtraAdvancedGraphicsOptionsPatched = true;
		}
		TXN_CATCH();
	}
	TXN_CATCH();

	// Additional Graphics options
	// FOV Control, Split Screen, Digital tacho
	// Requires patches: Registry (for saving/loading)
	if (HasRegistry) try
	{
		using namespace NewGraphicsOptions;

		bool HasPatches_FOV = false;
		try
		{
			auto cameras_after_initialise = get_pattern("7C E5 68 ? ? ? ? E8", 7);

			void* exterior_fov[] = {
				get_pattern("E8 ? ? ? ? 57 E8 ? ? ? ? 8B 45 08"),
				get_pattern("E8 ? ? ? ? 85 FF 74 06 57 E8 ? ? ? ? 8B 45 08 85 C0 74 0B 6A 05"),
			};

			void* interior_fov[] = {
				get_pattern("E8 ? ? ? ? 85 FF 74 06 57 E8 ? ? ? ? 8B 45 08 85 C0 74 0B 6A 04"),
			};

			for (void* addr : exterior_fov)
			{
				InjectHook(addr, Camera_SetFOV_Exterior);
			}
			for (void* addr : interior_fov)
			{
				InjectHook(addr, Camera_SetFOV_Interior);
			}

			InterceptCall(cameras_after_initialise, orgCameras_AfterInitialise, Cameras_AfterInitialise);

			HasPatches_FOV = true;
		}
		TXN_CATCH();

		bool HasPatches_SplitScreenOption = false;
		try
		{
			auto get_widescreen_init = get_pattern("E8 ? ? ? ? E8 ? ? ? ? 25 ? ? ? ? 8B F0", 5);

			auto get_widescreen1 = pattern("E8 ? ? ? ? F6 D8 1B C0 40 8B F0").count(2);
			void* get_widescreen[] = {
				ReadCallFrom(get_pattern("E8 ? ? ? ? 8B 75 08 89 44 24 3C")),
				get_pattern("E8 ? ? ? ? F6 D8 1B C0 40 83 FE 02"),
				get_pattern("E8 ? ? ? ? F6 D8 1B C0 40 8B F8"),
				get_pattern("E8 ? ? ? ? F6 D8 1B C0 6A 01 40"),
				get_pattern("E8 ? ? ? ? 84 C0 75 08"),
				get_pattern("E8 ? ? ? ? F6 D8 1B C0 68"),
				get_pattern("E8 ? ? ? ? 8B 9C 24 ? ? ? ? F6 D8"), // Unsure
				get_pattern("75 1C E8 ? ? ? ? 85 C0 75 07", 2),
				get_widescreen1.get(0).get<void>(),
				get_widescreen1.get(1).get<void>(),
			};

			InjectHook(get_widescreen_init, IsVerticalSplitscreen_ReadOption);
			for (void* addr : get_widescreen)
			{
				InjectHook(addr, IsVerticalSplitscreen);
			}

			HasPatches_SplitScreenOption = true;
		}
		TXN_CATCH();

		bool HasPatches_DigitalTacho = false;
		if (HasGameInfo && HasUIPositions && HasGraphics) try
		{
			std::array<void*, 5> num_of_players_in_race = {
				get_pattern("E8 ? ? ? ? 3C 01 75 3D"),
				get_pattern("E8 ? ? ? ? 3C 02 75 15"),
				get_pattern("E8 ? ? ? ? 3C 01 75 29"),
				get_pattern("E8 ? ? ? ? 3C 01 0F 85 ? ? ? ? 6A 01"),
				get_pattern("E8 ? ? ? ? 3C 02 75 3B 8B 44 24 2C"),
			};

			std::array<void*, 2> get_widescreen = {
				get_pattern("E8 ? ? ? ? F6 D8 1B C0 68"),
				get_pattern("E8 ? ? ? ? F6 D8 1B C0 6A 01 40"),
			};

			auto draw_tacho = get_pattern("E8 ? ? ? ? 6A 1C");

			HookEach_GetNumberOfPlayersInThisRace(num_of_players_in_race, InterceptCall);
			HookEach_GetWidescreen(get_widescreen, InterceptCall);

			InterceptCall(draw_tacho, orgDrawTacho, DrawTacho_SetPosition);

			HasPatches_DigitalTacho = true;
		}
		TXN_CATCH();

		// Only make frontend changes if all functionality has been successfully patched in
		if (HasGlobals && HasCMR3FE && HasCMR3Font && HasMenuHook && HasLanguageHook && HasPatches_FOV && HasPatches_SplitScreenOption && HasPatches_DigitalTacho) try
		{	
			void* back_locals;
			void* graphics_display_jump_table_num;
			void*** graphics_display_jump_table_ptr;
			void* graphics_display_new_case = &PC_GraphicsOptions_Display_CaseNewOptions;
			try
			{
				// EFIGS/Polish
				back_locals = get_pattern("8D 85 ? ? ? ? 50 E8 ? ? ? ? BA", 2);
				PC_GraphicsOptions_Display_NewOptionsJumpBack = get_pattern("0F BF 55 18 3B DA 75 07");

				auto graphics_display_jump_table = pattern("83 FB 05 0F 87").get_one();
				graphics_display_jump_table_num = graphics_display_jump_table.get<void>(2);
				graphics_display_jump_table_ptr = graphics_display_jump_table.get<void**>(9 + 3);

			}
			catch (const hook::txn_exception&)
			{
				// Czech
				back_locals = get_pattern("8D 85 ? ? ? ? 50 E8 ? ? ? ? 8B F8 83 C9 FF", 2);
				PC_GraphicsOptions_Display_NewOptionsJumpBack = get_pattern("0F BF 45 ? 39 44 24");

				auto graphics_display_jump_table = pattern("89 4C 24 ? 83 F8 05").get_one();
				graphics_display_jump_table_num = graphics_display_jump_table.get<void>(4 + 2);
				graphics_display_jump_table_ptr = graphics_display_jump_table.get<void**>(13 + 3);

				graphics_display_new_case = &PC_GraphicsOptions_Display_CaseNewOptions_Czech;
			}

			auto graphics_enter_new_options = get_pattern("89 86 ? ? ? ? E8 ? ? ? ? 5E", 6 + 5 + 1);
			auto graphics_exit_new_options = get_pattern("8B 86 ? ? ? ? 50 E8 ? ? ? ? E8", 0x17);

			// Build and inject a new jump table
			Patch<uint8_t>(graphics_display_jump_table_num, EntryID::GRAPHICS_NUM - 1);

			void** orgJumpTable = *graphics_display_jump_table_ptr;
			static const void* graphics_display_new_jump_table[EntryID::GRAPHICS_NUM] = {
				orgJumpTable[0], orgJumpTable[1], orgJumpTable[2], orgJumpTable[3],
				graphics_display_new_case, graphics_display_new_case, graphics_display_new_case, graphics_display_new_case,
				orgJumpTable[4], orgJumpTable[5]
			};
			Patch(graphics_display_jump_table_ptr, &graphics_display_new_jump_table);

			Patch<uint32_t>(back_locals, offsetof(MenuDefinition, m_entries[EntryID::GRAPHICS_BACK]));

			InjectHook(graphics_enter_new_options, PC_GraphicsOptions_Enter_NewOptions, PATCH_JUMP);
			InjectHook(graphics_exit_new_options, PC_GraphicsOptions_Exit_NewOptions, PATCH_JUMP);

			Menus::Patches::ExtraGraphicsOptionsPatched = true;
		}
		TXN_CATCH();
	}
	TXN_CATCH();


	// Disable teleports if an INI option is specified
	if (HasRegistry && Registry::GetRegistryDword(Registry::ADVANCED_SECTION_NAME, Registry::NO_TELEPORTS_KEY_NAME).value_or(0) != 0) try
	{
		auto flash_screen_white = get_pattern("56 33 F6 39 B1", -7);

		Patch<uint8_t>(flash_screen_white, 0xC3);
	}
	TXN_CATCH();


	// SP text on the Start screen
	// Make sure this is always the last patch, just in case
	if (HasGraphics && HasCMR3Font) try
	{
		using namespace SPText;

		std::array<void*, 2> texts = {
			get_pattern("50 E8 ? ? ? ? EB 08", 1),
			get_pattern("51 E8 ? ? ? ? 5B 83 C4 08", 1)
		};

		HookEach(texts, InterceptCall);
	}
	TXN_CATCH();


	// Blog link in Secrets screen
	try
	{
		// English/Czech only, Polish uses a translation string
		auto codemasters_url = get_pattern("68 13 01 00 00 68 40 01 00 00 68 ? ? ? ? 6A 0C E8", 10 + 1);
		Patch(codemasters_url, BONUSCODES_URL);
	}
	TXN_CATCH();

	
	// Install the locale pack (if applicable)
	ApplyMergedLocalizations(HasRegistry, HasFrontEnd, HasGameInfo, HasLanguageHook, HasKeyboard, HasTexture, HasCMR3Font);
}

void OnInitializeHook()
{
	const bool HasRegistry = Registry::Init();

	if (Version::DetectVersion(HasRegistry))
	{
		ApplyPatches(HasRegistry);
	}
}
