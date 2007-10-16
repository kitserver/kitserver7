/* KitServer Settings Patcher */

#define UNICODE

#include <windows.h>
#include "shared.h"
#include "manage.h"
#include "imageutil.h"
#include "detect.h"
#include "kset.h"
#include "kset_addr.h"
#include "dllinit.h"
#include "configs.h"
#include "lang.h"
#define lang(s) _getTransl("kset",s)

// VARIABLES
HINSTANCE hInst = NULL;
int controllerCount = 0;
bool allQualities = true;
wchar_t myDir[BUFLEN];
wchar_t ksetLang[64] = L"eng\0";

// FUNCTIONS
void hookFunctions();



void ksetConfig(char* pName, const void* pValue, DWORD a) {
	switch (a) {
		case 1:	// lang
			wcscpy(ksetLang, (wchar_t*)pValue);
			break;
	}
}

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
    
    ZeroMemory(myDir, WBUFLEN);
		GetModuleFileName(hInst, myDir, BUFLEN);
		wchar_t *q = myDir + wcslen(myDir);
		while ((q != myDir) && (*q != '\\')) { *q = '\0'; q--; }
    
    // read configuration
		wchar_t temp[BUFLEN];
		ZeroMemory(temp, WBUFLEN);
		wcscpy(temp, myDir); 
		wcscat(temp, L"config.txt");
		if (!readConfig(temp))
			;//TRACE(L"Couldn't open the config.txt!");
    
		_getConfig("kload", "lang", DT_STRING, 1, ksetConfig);
		
		ZeroMemory(temp, WBUFLEN);
		swprintf(temp, L"%s.\\lang_%s.txt", myDir, ksetLang);
    readLangFile(temp, hInstance);
    
    allQualities = (MessageBox(0, lang("MsgEnableAllQual"), WINDOW_TITLE, MB_YESNO) == IDYES);
    
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