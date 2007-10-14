/* KitServer Loader */
#define UNICODE
#define THISMOD &k_kload

#include <windows.h>
#include "shared.h"
#include "manage.h"
#include "kload_config.h"
#include "log.h"
#include "imageutil.h"
#include "detect.h"
#include "kload.h"
#include "kload_addr.h"
#include "dllinit.h"
//#include "hook.h"

#include "d3dx9tex.h"

#include <map>
#include <hash_map>
#include <vector>


// VARIABLES
HINSTANCE hInst = NULL;
KMOD k_kload = {MODID, NAMELONG, NAMESHORT, DEFAULT_DEBUG};
PESINFO g_pesinfo = {
	NULLSTRING,				// myDir
	DEFAULT_GDB_DIR,	// gdbDir
	NULLSTRING,				// logName
	-1,								// gameVersion
};
extern wchar_t* GAME[];
vector <wchar_t[512]> dlls;
vector <wchar_t[512]>::iterator dllsIt;

// FUNCTIONS
void setPesInfo();

/*******************/
/* DLL Entry Point */
/*******************/
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		hInst = hInstance;
		
		setPesInfo();
		
		OpenLog(g_pesinfo.logName);
		TRACE(L"Log started.");
		RegisterKModule(THISMOD);
		
		if (!checkGameVersion()) {
			TRACE(L"Sorry, your game version isn't supported!");
			return false;
		}
		copyAdresses();
		
		/*
		// read configuration
		wchar_t cfgFile[BUFLEN];
		ZeroMemory(cfgFile, WBUFLEN);
		wcscpy(cfgFile, g_pesinfo.mydir); 
		wcscat(cfgFile, CONFIG_FILE);
		ReadConfig(&g_config, cfgFile);
		*/
		
		// adjust gdbDir, if it is specified as relative path
		if (g_pesinfo.gdbDir[0] == '.')
		{
			// it's a relative path. Therefore do it relative to mydir
			wchar_t temp[BUFLEN];
			ZeroMemory(temp, WBUFLEN);
			wcscpy(temp, g_pesinfo.myDir); 
			wcscat(temp, g_pesinfo.gdbDir);
			
			ZeroMemory(g_pesinfo.gdbDir, WBUFLEN);
			wcscpy(g_pesinfo.myDir, temp);
		}
		
		#ifdef MYDLL_RELEASE_BUILD
		//if debugging shouldn't have been enabled, delete log file
		if (k_kload.debug < 1) {
			CloseLog();
			DeleteFile(g_pesinfo.logName);
		}
		#endif
		
		TRACE1S(L"Game version: %s", GAME[g_pesinfo.gameVersion]);
		//initAddresses(g_pesinfo.GameVersion);
		
		// load libs
		for (dllsIt = dlls.begin(); dllsIt != dlls.end(); dllsIt++) {
	  	TRACE1S(L"Loading module \"%s\" ...", *dllsIt);
	  	if (LoadLibrary(*dllsIt) == NULL)
	  		TRACE(L"... was NOT successful!");
	  	else
	  		TRACE(L"... was successful!");
    }
    
    //hookDirect3DCreate9();
	}
	
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		TRACE(L"Closing log.");
		CloseLog();
	}
	
	return true;
}



KEXPORT PESINFO* getPesInfo()
{
	return &g_pesinfo;
}

void setPesInfo()
{
	wcscpy(g_pesinfo.logName, L".\\test.log");
	
	g_pesinfo.gameVersion = GetGameVersion();
	return;
}

KEXPORT void RegisterKModule(KMOD *k)
{
	TRACE2S(L"Registering module %s (\"%s\")", k->nameShort, k->nameLong);
	return;
}
