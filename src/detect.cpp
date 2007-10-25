// detect.cpp

#include "windows.h"
#include <stdio.h>
#include "detect.h"
#include "imageutil.h"

wchar_t* GAME[] = { 
	L"PES2008 PC DEMO",
	L"[Settings] PES2008 PC DEMO",
	L"PES2008 PC",
	L"[Settings] PES2008 PC",
	L"PES2008 PC FLT-NODVD",
};
char* GAME_GUID[] = {
	"Pro Evolution Soccer 2008 DEMO",
	"Pro Evolution Soccer 2008 DEMO",
	"Pro Evolution Soccer 2008",
	"Pro Evolution Soccer 2008",
    "rr0\"\x0d\x09\x08",
};
DWORD GAME_GUID_OFFSETS[] = { 0x67aca8, 0x5b5c4, 0x994e74, 0x5ec34, 0x3e0 };
bool ISGAME[] = { true, false };

// Returns the game version id
int GetGameVersion(void)
{
	HMODULE hMod = GetModuleHandle(NULL);
	for (int i=0; i<sizeof(GAME_GUID)/sizeof(char*); i++)
	{
		if (!IsBadReadPtr((BYTE*)hMod + GAME_GUID_OFFSETS[i], strlen(GAME_GUID[i]))) {
			char* guid = (char*)((DWORD)hMod + GAME_GUID_OFFSETS[i]);
			if (memcmp(guid, GAME_GUID[i], strlen(GAME_GUID[i]))==0)
				return i;
		} 
	}
	return -1;
}

// Returns the game version id
int GetGameVersion(wchar_t* filename)
{
	char guid[512];
    memset(guid,0,sizeof(guid));

	FILE* f = _wfopen(filename, L"rb");
	if (f == NULL)
		return -1;

	// check for regular exes
	for (int i=0; i<sizeof(GAME_GUID)/sizeof(char*); i++)
	{
		fseek(f, GAME_GUID_OFFSETS[i], SEEK_SET);
		fread(guid, strlen(GAME_GUID[i]), 1, f);
		if (memcmp(guid, GAME_GUID[i], strlen(GAME_GUID[i]))==0)
		{
			fclose(f);
			return i;
		}
	}

	// unrecognized.
	return -1;
}

bool isGame(int gameVersion) {
	return ISGAME[gameVersion % 2];
}
