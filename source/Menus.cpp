#include "Menus.h"

#include <cstring>

MenuDefinition* gmoFrontEndMenus;

const bool bPolishExecutable = false;

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
		auto* advGraphicsFSAA = &gmoFrontEndMenus[MenuID::GRAPHICS_ADVANCED].m_entries[8];
		auto* tempDest = &gmoFrontEndMenus[MenuID::GRAPHICS_ADVANCED].m_entries[11];
		memmove(tempDest, advGraphicsFSAA, 3 * sizeof(*advGraphicsFSAA));
		gmoFrontEndMenus[MenuID::GRAPHICS_ADVANCED].m_numEntries = 12;
	}
}
