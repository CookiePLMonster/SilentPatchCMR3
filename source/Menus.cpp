#include "Menus.h"

void (*orgMenu_SetUpEntries)(int);
MenuDefinition* gMenus;

void Menu_SetUpEntries_Patched(int languagesOnly)
{
	orgMenu_SetUpEntries(languagesOnly);

	// TODO: Check for Polish EXE
	
	// Re-enable Languages screen
	gMenus[17].m_entries[3].m_canBeSelected = 1;
	gMenus[17].m_entries[3].m_isDisplayed = 1;


	// TODO: Do this only if there are multiple languages installed
	// Re-enable TEXT

	if (languagesOnly == 0)
	{
		// Same as the original hack, but done only once to fix an original animations bug
		gMenus[29].m_curScrollTopEntry = 1;
		gMenus[29].m_curEntry = 1;
	}
}
