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
	L"PES2008 PC 1.10",
	L"PES2008 PC 1.10 NODVD",
	L"PES2008 PC 1.20",
};
char* GAME_GUID[] = {
	"Pro Evolution Soccer 2008 DEMO",
	"Pro Evolution Soccer 2008 DEMO",
	"Pro Evolution Soccer 2008",
	"Pro Evolution Soccer 2008",
    "rr0\"\x0d\x09\x08",
    "PC  1.10",
    "PC  1.10",
    "PC  1.20",
};
DWORD GAME_GUID_OFFSETS[] = { 0x67aca8, 0x5b5c4, 0x994e74, 0x5ec34, 0x3e0, 0x977c50, 0x977c50, 0x978be8 };
bool ISGAME[] = { true, false, true, false, true, true, true, true };
BYTE BASE_GAME[] = {0, 1, 2, 3, 2, 5, 5, 7};

// Returns the game version id
int GetRealGameVersion(void)
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
int GetRealGameVersion(wchar_t* filename)
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

bool isGame(int gameVersion)
{
	if (gameVersion == -1) return false;
	return ISGAME[gameVersion];
}

bool isRealGame(int realGameVersion)
{
	if (realGameVersion == -1) return false;
	return ISGAME[GetGameVersion(realGameVersion)];
}

int GetGameVersion()
{
	int v = GetRealGameVersion();
	if (v == -1) return -1;
	return BASE_GAME[v];
}

int GetGameVersion(int realGameVersion)
{
	if (realGameVersion == -1) return -1;
	return BASE_GAME[realGameVersion];
}
