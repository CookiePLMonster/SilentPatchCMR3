#include "Menus.h"

#include "Globals.h"
#include "Graphics.h"
#include "Language.h"
#include "Registry.h"

#include <cstring>

MenuDefinition* gmoFrontEndMenus;

const bool bPolishExecutable = false;

static uint32_t GetValueDefault(uint32_t val, uint32_t defValue)
{
	return val != 0 ? val : defValue;
}

static int32_t GetResolutionEntryFormatID(const MenuResolutionEntry* entry)
{
	switch (entry->m_format)
	{
	case D3DFMT_R8G8B8:
		return 1;
	case D3DFMT_A8R8G8B8:
	case D3DFMT_X8R8G8B8:
		return 2;
	case D3DFMT_R5G6B5:
	case D3DFMT_X1R5G5B5:
	case D3DFMT_A1R5G5B5:
		return 0;
	default:
		break;
	}
	return -1;
}

void DrawLeftRightArrows_RightAlign(MenuDefinition* menu, uint32_t entryID, float interp, int leftArrow, int rightArrow, uint32_t posY)
{
	const float scaledWidth = GetScaledResolutionWidth();
	
	DrawLeftRightArrows(menu, entryID, interp, static_cast<int>(scaledWidth + leftArrow - 640), static_cast<int>(scaledWidth + rightArrow - 640), posY);
}

void FrontEndMenuSystem_SetupMenus_Custom(int languagesOnly)
{
	// TODO: Check for Polish EXE
	if (bPolishExecutable)
	{
		// Re-enable Languages screen
		gmoFrontEndMenus[17].m_entries[3].m_canBeSelected = 1;
		gmoFrontEndMenus[17].m_entries[3].m_isDisplayed = 1;


		// TODO: Do this only if there are multiple languages installed
		// Re-enable TEXT

		if (languagesOnly == 0)
		{
			// Same as the original hack, but done only once to fix an original animations bug
			gmoFrontEndMenus[29].m_curScrollTopEntry = 1;
			gmoFrontEndMenus[29].m_curEntry = 1;
		}
	}

	// Expanded Advanced Graphics
	if (Menus::Patches::ExtraAdvancedGraphicsOptionsPatched)
	{
		auto* advGraphicsShadows = &gmoFrontEndMenus[MenuID::GRAPHICS_ADVANCED].m_entries[3];
		auto* tempDest = &gmoFrontEndMenus[MenuID::GRAPHICS_ADVANCED].m_entries[4];
		memmove(tempDest, advGraphicsShadows, (11 - 3) * sizeof(*advGraphicsShadows));

		gmoFrontEndMenus[MenuID::GRAPHICS_ADVANCED].m_entries[EntryID::GRAPHICS_ADV_DISPLAYMODE].m_entryDataInt = 3;

		gmoFrontEndMenus[MenuID::GRAPHICS_ADVANCED].m_numEntries = EntryID::GRAPHICS_ADV_NUM;
	}
}

uint32_t PC_GraphicsAdvanced_ComputePresetQuality()
{
	using namespace EntryID;

	const auto& menu = gmoFrontEndMenus[MenuID::GRAPHICS_ADVANCED];
	uint32_t GraphicsQuality = 3; // Custom
	if (menu.m_entries[GRAPHICS_ADV_TEXTUREQUALITY].m_value == 0 &&
		menu.m_entries[GRAPHICS_ADV_ENVMAP].m_value == 0 &&
		menu.m_entries[GRAPHICS_ADV_SHADOWS].m_value == 0 &&
		menu.m_entries[GRAPHICS_ADV_DRAWDISTANCE].m_value == 0)
	{
		GraphicsQuality = 0; // Loe
	}
	else if (menu.m_entries[GRAPHICS_ADV_TEXTUREQUALITY].m_value == 1 &&
		menu.m_entries[GRAPHICS_ADV_ENVMAP].m_value == 0 &&
		menu.m_entries[GRAPHICS_ADV_SHADOWS].m_value == 1 &&
		menu.m_entries[GRAPHICS_ADV_DRAWDISTANCE].m_value == 5)
	{
		GraphicsQuality = 1; // Medium
	}
	else if (menu.m_entries[GRAPHICS_ADV_TEXTUREQUALITY].m_value == 1 &&
		menu.m_entries[GRAPHICS_ADV_ENVMAP].m_value == 1 &&
		menu.m_entries[GRAPHICS_ADV_SHADOWS].m_value == 1 &&
		menu.m_entries[GRAPHICS_ADV_DRAWDISTANCE].m_value == 9)
	{
		GraphicsQuality = 2; // High
	}

	return GraphicsQuality;
}

void PC_GraphicsAdvanced_SetGraphicsFromPresetQuality()
{
	using namespace EntryID;

	auto& menu = gmoFrontEndMenus[MenuID::GRAPHICS_ADVANCED];
	const uint32_t GraphicsQuality = CMR_FE_GetGraphicsQuality();
	if (GraphicsQuality == 0) // Low
	{
		menu.m_entries[GRAPHICS_ADV_TEXTUREQUALITY].m_value = 0;
		menu.m_entries[GRAPHICS_ADV_ENVMAP].m_value = 0;
		menu.m_entries[GRAPHICS_ADV_SHADOWS].m_value = 0;
		menu.m_entries[GRAPHICS_ADV_DRAWDISTANCE].m_value = 0;
	}
	else if (GraphicsQuality == 1) // Medium
	{
		menu.m_entries[GRAPHICS_ADV_TEXTUREQUALITY].m_value = 1;
		menu.m_entries[GRAPHICS_ADV_ENVMAP].m_value = 0;
		menu.m_entries[GRAPHICS_ADV_SHADOWS].m_value = 1;
		menu.m_entries[GRAPHICS_ADV_DRAWDISTANCE].m_value = 5;
	}
	else if (GraphicsQuality == 2) // High
	{
		menu.m_entries[GRAPHICS_ADV_TEXTUREQUALITY].m_value = 1;
		menu.m_entries[GRAPHICS_ADV_ENVMAP].m_value = 1;
		menu.m_entries[GRAPHICS_ADV_SHADOWS].m_value = 1;
		menu.m_entries[GRAPHICS_ADV_DRAWDISTANCE].m_value = 9;
	}

	CMR_FE_SetTextureQuality(menu.m_entries[GRAPHICS_ADV_TEXTUREQUALITY].m_value);
	CMR_FE_SetEnvironmentMap(menu.m_entries[GRAPHICS_ADV_ENVMAP].m_value);
	CMR_FE_SetDrawShadow(menu.m_entries[GRAPHICS_ADV_SHADOWS].m_value);
	CMR_FE_SetDrawDistance(static_cast<uint32_t>(menu.m_entries[GRAPHICS_ADV_DRAWDISTANCE].m_value * 19.444444f + 80.0f));
}

void PC_GraphicsAdvanced_LoadSettings()
{
	const int32_t Adapter = Registry::GetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"ADAPTER");
	const uint32_t NumAdapters = Graphics_GetNumAdapters();

	// TODO: Desktop resolution
	const uint32_t Width = GetValueDefault(Registry::GetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"WIDTH"), 640);
	const uint32_t Height = GetValueDefault(Registry::GetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"HEIGHT"), 480);

	const Graphics_Config& config = Graphics_GetCurrentConfig();
	Graphics_CheckForVertexShaders(config.m_adapter, 0, 1);

	uint32_t BitDepth;
	switch (config.m_format)
	{
	case D3DFMT_R8G8B8:
		BitDepth = 1;
		break;
	case D3DFMT_A8R8G8B8:
	case D3DFMT_X8R8G8B8:
		BitDepth = 2;
		break;
	case D3DFMT_R5G6B5:
	case D3DFMT_X1R5G5B5:
	case D3DFMT_A1R5G5B5:
	case D3DFMT_A4R4G4B4:
		BitDepth = 0;
		break;
	default:
		BitDepth = 0;
		break;
	}

	using namespace EntryID;
	auto& menu = gmoFrontEndMenus[MenuID::GRAPHICS_ADVANCED];
	menu.m_entries[GRAPHICS_ADV_DRIVER].m_value = Adapter;
	menu.m_entries[GRAPHICS_ADV_RESOLUTION].m_value = CMR_GetValidModeIndex(Adapter, Width, Height, BitDepth);
	menu.m_entries[GRAPHICS_ADV_ZDEPTH].m_value = Registry::GetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"ZDEPTH");
	menu.m_entries[GRAPHICS_ADV_FSAA].m_value = Registry::GetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"FSAA");
	menu.m_entries[GRAPHICS_ADV_GAMMA].m_value = Registry::GetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"GAMMA");

	// New SP options
	menu.m_entries[GRAPHICS_ADV_DISPLAYMODE].m_value = Registry::GetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"DISPLAY_MODE");

	menu.m_entries[GRAPHICS_ADV_TEXTUREQUALITY].m_value = CMR_FE_GetTextureQuality();
	menu.m_entries[GRAPHICS_ADV_ENVMAP].m_value = CMR_FE_GetEnvironmentMap();
	menu.m_entries[GRAPHICS_ADV_SHADOWS].m_value = CMR_FE_GetDrawShadow();

	Graphics_SetGammaRamp(0, menu.m_entries[GRAPHICS_ADV_GAMMA].m_value * 0.6111111f + 0.5f);
	menu.m_entries[GRAPHICS_ADV_DRAWDISTANCE].m_value = static_cast<int>((CMR_FE_GetDrawDistance() - 80.0f) / 19.44444f + 0.5f);

	menu.m_entries[0].m_entryDataInt = NumAdapters;
	menu.m_entries[1].m_entryDataInt = CMR_ValidateModeFormats(Adapter);
}

void PC_GraphicsAdvanced_SaveSettings()
{
	using namespace EntryID;
	const auto& menu = gmoFrontEndMenus[MenuID::GRAPHICS_ADVANCED];

	const int32_t AdapterProductID = CMR_GetAdapterProductID(menu.m_entries[GRAPHICS_ADV_DRIVER].m_value, -1);
	const int32_t AdapterVendorID = CMR_GetAdapterVendorID(menu.m_entries[GRAPHICS_ADV_DRIVER].m_value, -1);
	const MenuResolutionEntry* ResolutionEntry = GetMenuResolutionEntry(menu.m_entries[GRAPHICS_ADV_DRIVER].m_value, menu.m_entries[GRAPHICS_ADV_RESOLUTION].m_value);
	const int32_t ResolutionEntryFormatID = GetResolutionEntryFormatID(ResolutionEntry);

	Packed_Registry registry;
	registry.adapterID = static_cast<uint8_t>(menu.m_entries[GRAPHICS_ADV_DRIVER].m_value);
	registry.resolutionID = static_cast<uint8_t>(menu.m_entries[GRAPHICS_ADV_RESOLUTION].m_value);
	registry.backbufferFormatID = static_cast<uint8_t>(ResolutionEntryFormatID);
	registry.depthFormatID = static_cast<uint8_t>(menu.m_entries[GRAPHICS_ADV_ZDEPTH].m_value);
	CMR_FE_StoreRegistry(&registry);

	CMR_FE_SetGraphicsQuality(PC_GraphicsAdvanced_ComputePresetQuality());
	CMR_FE_SetTextureQuality(menu.m_entries[GRAPHICS_ADV_TEXTUREQUALITY].m_value);
	CMR_FE_SetEnvironmentMap(menu.m_entries[GRAPHICS_ADV_ENVMAP].m_value);
	CMR_FE_SetDrawShadow(menu.m_entries[GRAPHICS_ADV_SHADOWS].m_value);
	CMR_FE_SetFSAA(menu.m_entries[GRAPHICS_ADV_FSAA].m_value);
	CMR_FE_SetDrawDistance(static_cast<int>(menu.m_entries[GRAPHICS_ADV_DRAWDISTANCE].m_value * 19.44444444f + 80.0f));
	SetUseLowQualityTextures(menu.m_entries[GRAPHICS_ADV_TEXTUREQUALITY].m_value == 0);

	Registry::SetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"ADAPTER", menu.m_entries[GRAPHICS_ADV_DRIVER].m_value);
	Registry::SetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"ADAPTER_VID", AdapterVendorID);
	Registry::SetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"ADAPTER_PID", AdapterProductID);
	Registry::SetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"WIDTH", ResolutionEntry->m_width);
	Registry::SetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"HEIGHT", ResolutionEntry->m_height);
	Registry::SetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"BITDEPTH", ResolutionEntryFormatID);
	Registry::SetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"ZDEPTH", menu.m_entries[GRAPHICS_ADV_ZDEPTH].m_value);
	Registry::SetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"GAMMA", menu.m_entries[GRAPHICS_ADV_GAMMA].m_value);
	Registry::SetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"FSAA", menu.m_entries[GRAPHICS_ADV_FSAA].m_value);

	// New SP options
	Registry::SetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"DISPLAY_MODE", menu.m_entries[GRAPHICS_ADV_DISPLAYMODE].m_value);
}

static int gSavedDriver, gSavedResolution, gSavedZDepth, gSavedDisplayMode, gSavedFSAA;
void PC_GraphicsAdvanced_Enter_NewOptions(MenuDefinition* menu, int /*a2*/)
{
	using namespace EntryID;

	gSavedDriver = menu->m_entries[GRAPHICS_ADV_DRIVER].m_value;
	gSavedResolution = menu->m_entries[GRAPHICS_ADV_RESOLUTION].m_value;
	gSavedZDepth = menu->m_entries[GRAPHICS_ADV_ZDEPTH].m_value;
	gSavedDisplayMode = menu->m_entries[GRAPHICS_ADV_DISPLAYMODE].m_value;
	gSavedFSAA = menu->m_entries[GRAPHICS_ADV_FSAA].m_value;
}

MenuDefinition* PC_GraphicsAdvanced_Select_NewOptions(MenuDefinition* menu, MenuEntry* entry)
{
	using namespace EntryID;

	if (gSavedDriver != menu->m_entries[GRAPHICS_ADV_DRIVER].m_value)
	{
		return &gmoFrontEndMenus[22];
	}

	if (gSavedResolution != menu->m_entries[GRAPHICS_ADV_RESOLUTION].m_value
		|| gSavedZDepth != menu->m_entries[GRAPHICS_ADV_ZDEPTH].m_value
		|| gSavedDisplayMode != menu->m_entries[GRAPHICS_ADV_DISPLAYMODE].m_value
		|| gSavedFSAA != menu->m_entries[GRAPHICS_ADV_FSAA].m_value)
	{
		CMR_SetupRender();
		return entry->m_destMenu;
	}

	PC_GraphicsAdvanced_SaveSettings();
	return menu->m_prevMenu;
}

void PC_GraphicsAdvanced_Display_NewOptions(MenuDefinition* menu, float interp, uint32_t posY, uint32_t entryID)
{
	switch (entryID)
	{
	case EntryID::GRAPHICS_ADV_DISPLAYMODE:
	{
		char* target = gszTempString;
		char* const targetEnd = target+512;

		const uint32_t stringsByOption[] = { Language::FULLSCREEN, Language::WINDOWED, Language::BORDERLESS };
		target += sprintf_s(target, targetEnd - target, "%s: ", Language_GetString(Language::DISPLAY_MODE));
		const int leftArrowTextLength = CMR3Font_GetTextWidth(0, gszTempString);

		target += sprintf_s(target, targetEnd - target, " %s ", Language_GetString(stringsByOption[menu->m_entries[EntryID::GRAPHICS_ADV_DISPLAYMODE].m_value]));
		const int rightArrowTextLength = CMR3Font_GetTextWidth(0, gszTempString);

		DrawLeftRightArrows_RightAlign(menu, entryID, interp, leftArrowTextLength + 393, rightArrowTextLength + 393, posY);
		break;
	}
	default:
		break;
	}
}
