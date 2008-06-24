/* stadium module */
#define UNICODE
#define THISMOD &k_stad

#include <windows.h>
#include <stdio.h>
#include <sys/stat.h>
#include "kload_exp.h"
#include "afsio.h"
#include "stadium.h"
#include "stadium_addr.h"
#include "dllinit.h"
#include "gdb.h"
#include "pngdib.h"
#include "utf8.h"

#define lang(s) getTransl("stadium",s)

#include <map>
#include <hash_map>
#include <wchar.h>

#define SWAPBYTES(dw) \
    ((dw<<24 & 0xff000000) | (dw<<8  & 0x00ff0000) | \
    (dw>>8  & 0x0000ff00) | (dw>>24 & 0x000000ff))

//#define CREATE_FLAGS FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING
#define CREATE_FLAGS 0

enum {
    COMMON,
    DAY_SUMMER_MESH, DAY_SUMMER_TURF,
    DAY_WINTER_MESH, DAY_WINTER_TURF,
    EVENING_SUMMER_MESH, EVENING_SUMMER_TURF, 
    EVENING_WINTER_MESH, EVENING_WINTER_TURF,
    NIGHT_SUMMER_MESH, NIGHT_SUMMER_TURF,
    NIGHT_WINTER_MESH, NIGHT_WINTER_TURF,
};

// VARIABLES
HINSTANCE hInst = NULL;
KMOD k_stad = {MODID, NAMELONG, NAMESHORT, DEFAULT_DEBUG};

#define STAD_FIRST  45
#define STAD_LAST   712
#define STAD_SPAN   42
#define STAD_COMMON 40

bool _stadium_bins[726];

// FUNCTIONS
HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface);

void stadConfig(char* pName, const void* pValue, DWORD a);
bool stadGetFileInfo(DWORD afsId, DWORD binId, HANDLE& hfile, DWORD& fsize);
bool IsStadiumFile(DWORD binId);
int GetBinType(DWORD binId);

// FUNCTION POINTERS

/*******************/
/* DLL Entry Point */
/*******************/
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		hInst = hInstance;

		RegisterKModule(THISMOD);
		
		if (!checkGameVersion()) {
			LOG(L"Sorry, your game version isn't supported!");
			return false;
		}

		copyAdresses();
		hookFunction(hk_D3D_CreateDevice, initModule);
	}
	
	else if (dwReason == DLL_PROCESS_DETACH)
	{
	}
	
	return true;
}

void stadConfig(char* pName, const void* pValue, DWORD a)
{
	switch (a) {
		case 1: // debug
			k_stad.debug = *(DWORD*)pValue;
			break;
	}
	return;
}

HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface) {

    getConfig("stadium", "debug", DT_DWORD, 1, stadConfig);

	unhookFunction(hk_D3D_CreateDevice, initModule);

    // fill in BINs map
    ZeroMemory(_stadium_bins, sizeof(_stadium_bins));
    for (DWORD i=0; i<726; i++)
        if (GetBinType(i)!=-1)
            _stadium_bins[i] = true;

    // register callback
    afsioAddCallback(stadGetFileInfo);

	TRACE(L"Hooking done.");
    return D3D_OK;
}

bool inline IsStadiumFile(DWORD binId)
{
    return _stadium_bins[binId];
}

int GetBinType(DWORD binId)
{
    if (binId == STAD_COMMON)
        return COMMON;

    if (binId < STAD_FIRST || binId > STAD_LAST)
        return -1;  // not a stadium file

    switch ((binId - STAD_FIRST) % STAD_SPAN)
    {
        case 0: 
            return DAY_SUMMER_MESH;
        case 2:
            return DAY_SUMMER_TURF; 
        case 7: 
            return DAY_WINTER_MESH;
        case 9:
            return DAY_WINTER_TURF; 
        case 14: 
            return EVENING_SUMMER_MESH;
        case 16:
            return EVENING_SUMMER_TURF; 
        case 21: 
            return EVENING_WINTER_MESH;
        case 23:
            return EVENING_WINTER_TURF; 
        case 28: 
            return NIGHT_SUMMER_MESH;
        case 30:
            return NIGHT_SUMMER_TURF; 
        case 35: 
            return NIGHT_WINTER_MESH;
        case 37:
            return NIGHT_WINTER_TURF; 
    }

    return -1; // not a stadium file
}

/**
 * Simple file-check routine.
 */
bool OpenFileIfExists(const wchar_t* filename, HANDLE& handle, DWORD& size)
{
    TRACE1S(L"OpenFileIfExists:: %s", filename);
    handle = CreateFile(filename,           // file to open
                       GENERIC_READ,          // open for reading
                       FILE_SHARE_READ,       // share for reading
                       NULL,                  // default security
                       OPEN_EXISTING,         // existing file only
                       FILE_ATTRIBUTE_NORMAL | CREATE_FLAGS, // normal file
                       NULL);                 // no attr. template

    if (handle != INVALID_HANDLE_VALUE)
    {
        size = GetFileSize(handle,NULL);
        return true;
    }
    return false;
}

/**
 * AFSIO callback
 */
bool stadGetFileInfo(DWORD afsId, DWORD binId, HANDLE& hfile, DWORD& fsize)
{
    if (afsId != 4 || !IsStadiumFile(binId)) // fast check
        return false;

    LOG1N(L"binId = %d", binId);

    wchar_t filename[1024] = {0};
    swprintf(filename,L"%sGDB\\stadiums\\wembley_%d.bin", 
            getPesInfo()->gdbDir,
            (binId==40) ? 40 : (213+((binId-STAD_FIRST)%STAD_SPAN)));

    return OpenFileIfExists(filename, hfile, fsize);
}

