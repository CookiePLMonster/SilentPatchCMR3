#include "Menus.h"

#include "Globals.h"
#include "Graphics.h"
#include "Language.h"
#include "Registry.h"
#include "Version.h"

#include <cstring>

MenuDefinition* gmoFrontEndMenus;

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

static constexpr int FOV_MIN = 30;
static constexpr int FOV_MAX = 150;
static constexpr int FOV_STEP = 5;
static constexpr int FOV_NUM_VALUES = ((FOV_MAX - FOV_MIN) / FOV_STEP) + 1;

int CMR_FE_GetExteriorFOV()
{
	return Registry::GetRegistryDword(Registry::GRAPHICS_SECTION_NAME, Registry::EXTERIOR_FOV_KEY_NAME).value_or(75);
}

int CMR_FE_GetInteriorFOV()
{
	return Registry::GetRegistryDword(Registry::GRAPHICS_SECTION_NAME, Registry::INTERIOR_FOV_KEY_NAME).value_or(75);
}

void CMR_FE_SetExteriorFOV(int FOV)
{
	Registry::SetRegistryDword(Registry::GRAPHICS_SECTION_NAME, Registry::EXTERIOR_FOV_KEY_NAME, FOV);
}

void CMR_FE_SetInteriorFOV(int FOV)
{
	Registry::SetRegistryDword(Registry::GRAPHICS_SECTION_NAME, Registry::INTERIOR_FOV_KEY_NAME, FOV);
}

bool CMR_FE_GetVerticalSplitscreen()
{
	return Registry::GetRegistryDword(Registry::GRAPHICS_SECTION_NAME, Registry::SPLIT_SCREEN_KEY_NAME).value_or(0) != 0;
}

void CMR_FE_SetVerticalSplitscreen(bool vertical)
{
	Registry::SetRegistryDword(Registry::GRAPHICS_SECTION_NAME, Registry::SPLIT_SCREEN_KEY_NAME, vertical ? 1 : 0);
}

bool CMR_FE_GetDigitalTacho()
{
	return Registry::GetRegistryDword(Registry::GRAPHICS_SECTION_NAME, Registry::DIGITAL_TACHO_KEY_NAME).value_or(0) != 0;
}

void CMR_FE_SetDigitalTacho(bool digital)
{
	Registry::SetRegistryDword(Registry::GRAPHICS_SECTION_NAME, Registry::DIGITAL_TACHO_KEY_NAME, digital ? 1 : 0);
}

void DrawLeftRightArrows_RightAlign(MenuDefinition* menu, uint32_t entryID, float interp, int leftArrow, int rightArrow, uint32_t posY)
{
	const float scaledWidth = GetScaledResolutionWidth();
	
	DrawLeftRightArrows(menu, entryID, interp, static_cast<int>(scaledWidth + leftArrow - 640), static_cast<int>(scaledWidth + rightArrow - 640), posY);
}

void FrontEndMenuSystem_SetupMenus_Custom(int languagesOnly)
{
	if (Version::IsPolish() && Version::HasNickyGristFiles())
	{
		// Re-enable Languages screen
		gmoFrontEndMenus[17].m_entries[3].m_canBeSelected = 1;
		gmoFrontEndMenus[17].m_entries[3].m_isDisplayed = 1;


		// TODO: Do this only if there are multiple languages installed
		// Re-enable TEXT
		if (languagesOnly == 0)
		{
			// Same as the original hack, but done only once to fix an original animations bug
			//gmoFrontEndMenus[29].m_curScrollTopEntry = 1;
			//gmoFrontEndMenus[29].m_curEntry = 1;
		}
	}

	if (Menus::Patches::MultipleTextsPatched)
	{
		gmoFrontEndMenus[29].m_entries[0].m_canBeSelected = 1;
		gmoFrontEndMenus[29].m_entries[0].m_isDisplayed = 1;
		gmoFrontEndMenus[29].m_entries[0].m_entryDataInt = 7;
	}

	if (Menus::Patches::MultipleCoDriversPatched)
	{
		gmoFrontEndMenus[29].m_entries[1].m_entryDataInt = 7;
		if (Version::HasNickyGristFiles())
		{
			gmoFrontEndMenus[29].m_entries[1].m_entryDataInt++;
		}
	}

	// Extended Graphics screen
	if (Menus::Patches::ExtraGraphicsOptionsPatched)
	{
		auto& menu = gmoFrontEndMenus[MenuID::GRAPHICS];
		{
			// Make space for FOV control
			auto* optSource = &menu.m_entries[4];
			auto* optDest = optSource + 4;
			memmove(optDest, optSource, 2 * sizeof(*optSource));
		}

		memcpy(&menu.m_entries[EntryID::GRAPHICS_SPLIT_SCREEN], &menu.m_entries[0], sizeof(menu.m_entries[0]));
		menu.m_entries[EntryID::GRAPHICS_SPLIT_SCREEN].m_stringID = 353;
		menu.m_entries[EntryID::GRAPHICS_SPLIT_SCREEN].m_entryDataString = nullptr;

		memcpy(&menu.m_entries[EntryID::GRAPHICS_TACHO], &menu.m_entries[0], sizeof(menu.m_entries[0]));
		menu.m_entries[EntryID::GRAPHICS_TACHO].m_stringID = Language::TACHOMETER;
		menu.m_entries[EntryID::GRAPHICS_TACHO].m_entryDataString = nullptr;

		// Base off Graphics Quality as it's the closest
		memcpy(&menu.m_entries[EntryID::GRAPHICS_EXTERIOR_FOV], &menu.m_entries[2], sizeof(menu.m_entries[2]));
		menu.m_entries[EntryID::GRAPHICS_EXTERIOR_FOV].m_stringID = Language::EXTERIOR_FOV;
		menu.m_entries[EntryID::GRAPHICS_EXTERIOR_FOV].m_entryDataString = nullptr;
		menu.m_entries[EntryID::GRAPHICS_EXTERIOR_FOV].m_entryDataInt = FOV_NUM_VALUES;

		memcpy(&menu.m_entries[EntryID::GRAPHICS_INTERIOR_FOV], &menu.m_entries[2], sizeof(menu.m_entries[2]));
		menu.m_entries[EntryID::GRAPHICS_INTERIOR_FOV].m_stringID = Language::INTERIOR_FOV;
		menu.m_entries[EntryID::GRAPHICS_INTERIOR_FOV].m_entryDataString = nullptr;
		menu.m_entries[EntryID::GRAPHICS_INTERIOR_FOV].m_entryDataInt = FOV_NUM_VALUES;

		menu.m_numEntries = EntryID::GRAPHICS_NUM;
	}

	// Expanded Advanced Graphics
	if (Menus::Patches::ExtraAdvancedGraphicsOptionsPatched)
	{
		auto& menu = gmoFrontEndMenus[MenuID::GRAPHICS_ADVANCED];
		{
			// Make space for Anisotropic Filtering
			auto* optSource = &menu.m_entries[7];
			auto* optDest = optSource + 3;
			memmove(optDest, optSource, 4 * sizeof(*optSource));
		}
		{
			// Make space for Display Mode and VSync
			auto* optSource = &menu.m_entries[3];
			auto* optDest = optSource + 2;
			memmove(optDest, optSource, 4 * sizeof(*optSource));
		}

		menu.m_entries[EntryID::GRAPHICS_ADV_DISPLAYMODE].m_entryDataInt = 3;
		menu.m_entries[EntryID::GRAPHICS_ADV_VSYNC].m_entryDataInt = 2;
		menu.m_entries[EntryID::GRAPHICS_ADV_VSYNC].m_displayBothOnOff = 1;
		menu.m_entries[EntryID::GRAPHICS_ADV_ANISOTROPIC].m_entryDataInt = 1;

		menu.m_numEntries = EntryID::GRAPHICS_ADV_NUM;
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
	CMR_FE_SetAnisotropicLevel(menu.m_entries[GRAPHICS_ADV_ANISOTROPIC].m_value);
}

void PC_GraphicsAdvanced_LoadSettings()
{
	const int32_t Adapter = Registry::GetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"ADAPTER").value_or(0);
	const uint32_t NumAdapters = Graphics_GetNumAdapters();

	// TODO: Desktop resolution
	const uint32_t Width = Registry::GetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"WIDTH").value_or(640);
	const uint32_t Height = Registry::GetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"HEIGHT").value_or(480);

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
	menu.m_entries[GRAPHICS_ADV_ZDEPTH].m_value = Registry::GetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"ZDEPTH").value_or(0);
	menu.m_entries[GRAPHICS_ADV_FSAA].m_value = Registry::GetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"FSAA").value_or(0);
	menu.m_entries[GRAPHICS_ADV_GAMMA].m_value = Registry::GetRegistryDword(Registry::REGISTRY_SECTION_NAME, L"GAMMA").value_or(1);

	// New SP options
	menu.m_entries[GRAPHICS_ADV_DISPLAYMODE].m_value = Registry::GetRegistryDword(Registry::REGISTRY_SECTION_NAME, Registry::DISPLAY_MODE_KEY_NAME).value_or(0);
	menu.m_entries[GRAPHICS_ADV_VSYNC].m_value = Registry::GetRegistryDword(Registry::REGISTRY_SECTION_NAME, Registry::VSYNC_KEY_NAME).value_or(1);
	menu.m_entries[GRAPHICS_ADV_ANISOTROPIC].m_value = Registry::GetRegistryDword(Registry::REGISTRY_SECTION_NAME, Registry::ANISOTROPIC_KEY_NAME).value_or(0);

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
	CMR_FE_SetAnisotropicLevel(menu.m_entries[GRAPHICS_ADV_ANISOTROPIC].m_value);

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
	Registry::SetRegistryDword(Registry::REGISTRY_SECTION_NAME, Registry::DISPLAY_MODE_KEY_NAME, menu.m_entries[GRAPHICS_ADV_DISPLAYMODE].m_value);
	Registry::SetRegistryDword(Registry::REGISTRY_SECTION_NAME, Registry::VSYNC_KEY_NAME, menu.m_entries[GRAPHICS_ADV_VSYNC].m_value);
	Registry::SetRegistryDword(Registry::REGISTRY_SECTION_NAME, Registry::ANISOTROPIC_KEY_NAME, menu.m_entries[GRAPHICS_ADV_ANISOTROPIC].m_value);
}

static int gSavedDriver, gSavedResolution, gSavedZDepth, gSavedDisplayMode, gSavedFSAA, gSavedVSync, gSavedAF;
void PC_GraphicsAdvanced_Enter_NewOptions(MenuDefinition* menu, int /*a2*/)
{
	using namespace EntryID;

	PC_GraphicsAdvanced_PopulateFromCaps_NewOptions(menu, menu->m_entries[GRAPHICS_ADV_DRIVER].m_value, menu->m_entries[GRAPHICS_ADV_DRIVER].m_value);

	gSavedDriver = menu->m_entries[GRAPHICS_ADV_DRIVER].m_value;
	gSavedResolution = menu->m_entries[GRAPHICS_ADV_RESOLUTION].m_value;
	gSavedZDepth = menu->m_entries[GRAPHICS_ADV_ZDEPTH].m_value;
	gSavedDisplayMode = menu->m_entries[GRAPHICS_ADV_DISPLAYMODE].m_value;
	gSavedFSAA = menu->m_entries[GRAPHICS_ADV_FSAA].m_value;
	gSavedVSync = menu->m_entries[GRAPHICS_ADV_VSYNC].m_value;
	gSavedAF = menu->m_entries[GRAPHICS_ADV_ANISOTROPIC].m_value;
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
		|| gSavedFSAA != menu->m_entries[GRAPHICS_ADV_FSAA].m_value
		|| gSavedVSync != menu->m_entries[GRAPHICS_ADV_VSYNC].m_value
		|| gSavedAF != menu->m_entries[GRAPHICS_ADV_ANISOTROPIC].m_value)
	{
		CMR_SetupRender();
		return entry->m_destMenu;
	}

	PC_GraphicsAdvanced_SaveSettings();
	return menu->m_prevMenu;
}

void PC_GraphicsAdvanced_PopulateFromCaps_NewOptions(MenuDefinition* menu, uint32_t currentAdapter, uint32_t /*newAdapter*/)
{
	const int maxAF = CMR_FE_GetMaxAnisotropicLevel(currentAdapter);
	if (maxAF >= 2)
	{
		menu->m_entries[EntryID::GRAPHICS_ADV_ANISOTROPIC].m_entryDataInt = 1 + static_cast<int>(log2(maxAF));
		menu->m_entries[EntryID::GRAPHICS_ADV_ANISOTROPIC].m_canBeSelected = 1;
	}
	else
	{
		menu->m_entries[EntryID::GRAPHICS_ADV_ANISOTROPIC].m_entryDataInt = 1;
		menu->m_entries[EntryID::GRAPHICS_ADV_ANISOTROPIC].m_canBeSelected = 0;
	}
}

void PC_GraphicsAdvanced_Display_NewOptions(MenuDefinition* menu, float interp, uint32_t posY, uint32_t entryID, uint32_t offColor, uint32_t onColor)
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
	case EntryID::GRAPHICS_ADV_VSYNC:
	{
		sprintf_s(gszTempString, 512, "%s: ", Language_GetString(Language::VSYNC));
		PC_GraphicsAdvanced_DisplayOnOff(menu, gszTempString, Language_GetString(85), Language_GetString(84), posY, menu->m_entries[EntryID::GRAPHICS_ADV_VSYNC].m_value,
			offColor, onColor, interp, menu->m_entries[EntryID::GRAPHICS_ADV_VSYNC].m_displayBothOnOff != 0);
		break;
	}
	case EntryID::GRAPHICS_ADV_ANISOTROPIC:
	{
		char* target = gszTempString;
		char* const targetEnd = target+512;

		target += sprintf_s(target, targetEnd - target, "%s: ", Language_GetString(Language::ANISOTROPIC));
		const int leftArrowTextLength = CMR3Font_GetTextWidth(0, gszTempString);

		const int afValue = menu->m_entries[EntryID::GRAPHICS_ADV_ANISOTROPIC].m_value;
		if (afValue > 0)
		{
			target += sprintf_s(target, targetEnd - target, " %dX ", 1 << afValue);
		}
		else
		{
			target += sprintf_s(target, targetEnd - target, " %s ", Language_GetString(85));
		}
		const int rightArrowTextLength = CMR3Font_GetTextWidth(0, gszTempString);

		DrawLeftRightArrows_RightAlign(menu, entryID, interp, leftArrowTextLength + 393, rightArrowTextLength + 393, posY);
		break;
	}
	default:
		break;
	}
}

void PC_GraphicsAdvanced_DisplayOnOff(MenuDefinition* /*menu*/, const char* optionText, const char* offText, const char* onText, uint32_t posY, int value, uint32_t offColor, uint32_t onColor, float interp, bool displayBoth)
{
	int currentPosX = CMR3Font_GetTextWidth(0, optionText) + 393;
	if (value == 0)
	{
		std::swap(offColor, onColor);
	}

	char buf[256];
	sprintf_s(buf, " %s ", offText);
	if (displayBoth || value == 0)
	{
		CMR3Font_BlitText_RightAlign(0, buf, static_cast<int16_t>(currentPosX), static_cast<int16_t>(posY), HandyFunction_AlphaCombineFlat(offColor, static_cast<uint32_t>(interp * interp * 255.0f)), 9);
		currentPosX += CMR3Font_GetTextWidth(0, buf);
	}

	if (displayBoth || value != 0)
	{
		sprintf_s(buf, displayBoth ?  "%s " : " %s ", onText);
		CMR3Font_BlitText_RightAlign(0, buf, static_cast<int16_t>(currentPosX), static_cast<int16_t>(posY), HandyFunction_AlphaCombineFlat(onColor, static_cast<uint32_t>(interp * interp * 255.0f)), 9);
	}
}

void PC_GraphicsOptions_Display_NewOptions(MenuDefinition* menu, float interp, uint32_t posY, uint32_t entryID, uint32_t offColor, uint32_t onColor)
{
	const uint8_t interpValue = static_cast<uint8_t>(interp * interp * 255.0f);
	switch (entryID)
	{
	case EntryID::GRAPHICS_SPLIT_SCREEN:
	{
		sprintf_s(gszTempString, 512, "%s: ", Language_GetString(menu->m_entries[entryID].m_stringID));
		const int splitScreenTextLength = CMR3Font_GetTextWidth(0, gszTempString);

		char buf[512];
		const char* horizontalText = Language_GetString(269);
		sprintf_s(buf, "%s ", horizontalText);
		const int horizontalTextLength = CMR3Font_GetTextWidth(0, buf);
		CMR3Font_BlitText_RightAlign(0, buf, static_cast<int16_t>(splitScreenTextLength + 393), static_cast<int16_t>(posY), HandyFunction_AlphaCombineFlat(offColor, interpValue), 9);

		CMR3Font_BlitText_RightAlign(0, Language_GetString(270), static_cast<int16_t>(splitScreenTextLength + horizontalTextLength + 393), static_cast<int16_t>(posY), HandyFunction_AlphaCombineFlat(onColor, interpValue), 9);
		break;
	}
	case EntryID::GRAPHICS_TACHO:
	{
		sprintf_s(gszTempString, 512, "%s: ", Language_GetString(menu->m_entries[entryID].m_stringID));
		const int splitScreenTextLength = CMR3Font_GetTextWidth(0, gszTempString);

		char buf[512];
		const char* horizontalText = Language_GetString(884);
		sprintf_s(buf, "%s ", horizontalText);
		const int horizontalTextLength = CMR3Font_GetTextWidth(0, buf);
		CMR3Font_BlitText_RightAlign(0, buf, static_cast<int16_t>(splitScreenTextLength + 393), static_cast<int16_t>(posY), HandyFunction_AlphaCombineFlat(offColor, interpValue), 9);

		CMR3Font_BlitText_RightAlign(0, Language_GetString(883), static_cast<int16_t>(splitScreenTextLength + horizontalTextLength + 393), static_cast<int16_t>(posY), HandyFunction_AlphaCombineFlat(onColor, interpValue), 9);
		break;
	}
	case EntryID::GRAPHICS_EXTERIOR_FOV:
	case EntryID::GRAPHICS_INTERIOR_FOV:
	{
		char* target = gszTempString;
		char* const targetEnd = target+512;

		target += sprintf_s(target, targetEnd - target, "%s: ", Language_GetString(menu->m_entries[entryID].m_stringID));
		const int leftArrowTextLength = CMR3Font_GetTextWidth(0, gszTempString);
		target += sprintf_s(target, targetEnd - target, " %d ", FOV_MIN + menu->m_entries[entryID].m_value * FOV_STEP);
		const int rightArrowTextLength = CMR3Font_GetTextWidth(0, gszTempString);

		DrawLeftRightArrows_RightAlign(menu, entryID, interp, leftArrowTextLength + 393, rightArrowTextLength + 393, posY);
		break;
	}
	default:
		break;
	}
}

void PC_GraphicsOptions_Enter_NewOptions(MenuDefinition* menu, int /*a2*/)
{
	menu->m_entries[EntryID::GRAPHICS_TACHO].m_value = CMR_FE_GetDigitalTacho();
	menu->m_entries[EntryID::GRAPHICS_SPLIT_SCREEN].m_value = CMR_FE_GetVerticalSplitscreen();
	menu->m_entries[EntryID::GRAPHICS_EXTERIOR_FOV].m_value = (CMR_FE_GetExteriorFOV() - FOV_MIN) / FOV_STEP;
	menu->m_entries[EntryID::GRAPHICS_INTERIOR_FOV].m_value = (CMR_FE_GetInteriorFOV() - FOV_MIN) / FOV_STEP;
}

void PC_GraphicsOptions_Exit_NewOptions(MenuDefinition* menu, int /*a2*/)
{
	CMR_FE_SetDigitalTacho(menu->m_entries[EntryID::GRAPHICS_TACHO].m_value != 0);
	CMR_FE_SetVerticalSplitscreen(menu->m_entries[EntryID::GRAPHICS_SPLIT_SCREEN].m_value != 0);
	CMR_FE_SetExteriorFOV(FOV_MIN + menu->m_entries[EntryID::GRAPHICS_EXTERIOR_FOV].m_value * FOV_STEP);
	CMR_FE_SetInteriorFOV(FOV_MIN + menu->m_entries[EntryID::GRAPHICS_INTERIOR_FOV].m_value * FOV_STEP);
}
