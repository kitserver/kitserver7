/* KitServer Loader */

#include <windows.h>
#include "imageutil.h"
#include "detect.h"
#include "kload.h"

// ADDRESSES
#define CODELEN 4
enum {
	C_CONTROLLERFIX1, C_CONTROLLERFIX2, C_QUALITYCHECK1, C_QUALITYCHECK2,
};

DWORD codeArray[][CODELEN] = { 
  // PES2008 DEMO
	{
		0x86dd12, 0x86dd22, 0x904b0f, 0x904bc5,
	},
	// [Settings] PES2008 PC DEMO
  {0,0,0,0,}
};

#define DATALEN 1
enum {
	DUMMY,	
};
DWORD dataArray[][DATALEN] = {
  // PES2008 DEMO
  {
  	0
  },
	// [Settings] PES2008 PC DEMO
	{0,}
};

DWORD code[CODELEN];
DWORD data[DATALEN];

// VARIABLES
HINSTANCE hInst = NULL;
bool allQualities = true;

// FUNCTIONS
void fixControllerLimit();


/*******************/
/* DLL Entry Point */
/*******************/
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		hInst = hInstance;
		int v = GetGameVersion();
		
		if (!isGame(v))
			return false;
		
		memcpy(code, codeArray[v], sizeof(code));
    memcpy(data, dataArray[v], sizeof(data));
    
    fixControllerLimit();
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		//
	}
	
	return true;
}


void fixControllerLimit() {
	DWORD protection = 0;
	DWORD newProtection = PAGE_EXECUTE_READWRITE;
	BYTE* bptr = NULL;
	
	if (code[C_CONTROLLERFIX1] != 0 && code[C_CONTROLLERFIX2] != 0)	{
		for (int i=0; i<2; i++) {
			bptr = (BYTE*)code[C_CONTROLLERFIX1 + i];
	    if (VirtualProtect(bptr, 7, newProtection, &protection)) {
	    	memset(bptr, 0x90, 7);
			}
		}
	}
	
	if (allQualities && code[C_QUALITYCHECK1] != 0 && code[C_QUALITYCHECK2] != 0)	{
		for (int i=0; i<2; i++) {
			bptr = (BYTE*)code[C_QUALITYCHECK1 + i];
	    if (VirtualProtect(bptr, 2, newProtection, &protection)) {
	    	/* NOP */ bptr[0] = 0x90;
	    	/* Jmp... */ bptr[1] = 0xe9;
			}
		}
	}
}
