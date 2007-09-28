// detect.cpp

#include "windows.h"
#include <stdio.h>
#include "detect.h"
#include "imageutil.h"

char* GAME[] = { 
	"PES2008 PC DEMO",
	"[Settings] PES2008 PC DEMO",
};
char* GAME_GUID[] = {
	"Pro Evolution Soccer 2008 DEMO",
	"Pro Evolution Soccer 2008 DEMO",
};
DWORD GAME_GUID_OFFSETS[] = { 0x67aca8, 0x5b5c4 };
bool ISGAME[] = { true, false };

// Returns the game version id
int GetGameVersion(void)
{
	HMODULE hMod = GetModuleHandle(NULL);
	for (int i=0; i<sizeof(GAME_GUID)/sizeof(char*); i++)
	{
		char* guid = (char*)((DWORD)hMod + GAME_GUID_OFFSETS[i]);
		if (strncmp(guid, GAME_GUID[i], lstrlen(GAME_GUID[i]))==0)
			return i;
	}
	return -1;
}

// Returns the game version id
int GetGameVersion(char* filename)
{
	char guid[] = "{00000000-0000-0000-0000-000000000000}";

	FILE* f = fopen(filename, "rb");
	if (f == NULL)
		return -1;

	// check for regular exes
	for (int i=0; i<sizeof(GAME_GUID)/sizeof(char*); i++)
	{
		fseek(f, GAME_GUID_OFFSETS[i], SEEK_SET);
		fread(guid, lstrlen(GAME_GUID[i]), 1, f);
		if (strncmp(guid, GAME_GUID[i], lstrlen(GAME_GUID[i]))==0)
		{
			fclose(f);
			return i;
		}
	}

	// unrecognized.
	return -1;
}

bool isGame(int gameVersion) {
	return ISGAME[gameVersion];
}