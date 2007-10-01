/* KitServer Settings Patcher */

#include <windows.h>
#include "imageutil.h"
#include "detect.h"
#include "shared.h"
#include "kset.h"

// ADDRESSES
#define CODELEN 4
enum {
	C_CONTROLLERADDED, C_BEFORECONTROLLERADD, C_SOMEFUNCTION, C_QUALITYCHECK, 
};

DWORD codeArray[][CODELEN] = { 
  // PES2008 DEMO
  {0,0,0,0,},
	// [Settings] PES2008 PC DEMO
	{
		0x4049c9, 0x4048be, 0x415aaf, 0x4159c0,
	}
};

#define DATALEN 1
enum {
	CONTROLLER_NUMBER,	
};
DWORD dataArray[][DATALEN] = {
  // PES2008 DEMO
  {0},
	// [Settings] PES2008 PC DEMO
	{
		0x45a6a2,
	}
};

DWORD code[CODELEN];
DWORD data[DATALEN];

// VARIABLES
HINSTANCE hInst = NULL;
int controllerCount = 0;
bool allQualities = true;


// FUNCTIONS
void hookFunctions();


/*******************/
/* DLL Entry Point */
/*******************/
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		hInst = hInstance;
		int v = GetGameVersion();
		
		if (isGame(v))
			return false;
		
		memcpy(code, codeArray[v], sizeof(code));
    memcpy(data, dataArray[v], sizeof(data));
    
    allQualities = (MessageBox(0, "Do you want to enable ALL quality levels, including those\n\
which aren't officially supported by your graphics card?", WINDOW_TITLE, MB_YESNO) == IDYES);
    
    hookFunctions();
    
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		//
	}
	
	return true;
}


void hookFunctions() {
	DWORD protection = 0;
	DWORD newProtection = PAGE_EXECUTE_READWRITE;
	BYTE* bptr = NULL;
	
	if (code[C_CONTROLLERADDED] != 0)	{
		bptr = (BYTE*)code[C_CONTROLLERADDED];
    if (VirtualProtect(bptr, 8, newProtection, &protection)) {
        bptr[0] = 0xe8;
        bptr[5] = 0x90; bptr[6] = 0x90; bptr[7] = 0x90;
        DWORD* ptr = (DWORD*)(code[C_CONTROLLERADDED] + 1);
        ptr[0] = (DWORD)ksetControllerAdded - (DWORD)(code[C_CONTROLLERADDED] + 5);
    }
    VirtualProtect((BYTE*)data[CONTROLLER_NUMBER], 1, newProtection, &protection);
	}

	if (allQualities && code[C_QUALITYCHECK] != 0) {
		bptr = (BYTE*)code[C_QUALITYCHECK];
    if (VirtualProtect(bptr, 4, newProtection, &protection)) {
        /* xor eax, eax */ bptr[0] = 0x33; bptr[1] = 0xc0;
        /* inc eax */ bptr[2] = 0x40;
        /* ret */ bptr[3] = 0xc3;
    }
	}
}


DWORD STDMETHODCALLTYPE ksetControllerAdded(DWORD p1) {
	controllerCount++;
	if (controllerCount < 4) {
		// add another controller
		*(BYTE*)data[CONTROLLER_NUMBER] = 0x31 + controllerCount;
		*(&p1 - 1) = code[C_BEFORECONTROLLERADD];		
		return 0;
	} else {
		
		controllerCount = 0;
		*(BYTE*)data[CONTROLLER_NUMBER] = 0x31;
		SOMEFUNCTION someFunction = (SOMEFUNCTION)code[C_SOMEFUNCTION];
		return someFunction(p1);
	}
}