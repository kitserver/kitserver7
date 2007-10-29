// lodmixer.cpp
#define UNICODE
#define THISMOD &k_lodmixer

#include <windows.h>
#include <stdio.h>
#include "kload_exp.h"
#include "lodmixer.h"
#include "lodmixer_addr.h"
#include "dllinit.h"

KMOD k_lodmixer={MODID,NAMELONG,NAMESHORT,DEFAULT_DEBUG};

HINSTANCE hInst;
DXCONFIG _dxconfig = {
    {DEFAULT_WIDTH, DEFAULT_HEIGHT},
};
UINT_PTR _timer = 0;

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved);

void initLodMixer();
void lodmixerConfig(char* pName, const void* pValue, DWORD a);

EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		TRACE(L"Attaching dll...");
		hInst=hInstance;
		RegisterKModule(&k_lodmixer);

		copyAdresses();
		hookFunction(hk_D3D_Create, initLodMixer);
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		TRACE(L"Detaching dll...");
	}
	return true;
}

void modSettings()
{
    *(DWORD*)data[SCREEN_WIDTH] = _dxconfig.screen.width;
    *(DWORD*)data[SCREEN_HEIGHT] = _dxconfig.screen.height;

    LOG2N(L"Resolution set: %dx%d", _dxconfig.screen.width, _dxconfig.screen.height);
}

void STDMETHODCALLTYPE lodmixerSetTransform(IDirect3DDevice9* self, 
		D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX* pMatrix)
{
    if (State == D3DTS_PROJECTION) {
        //TRACE1N(L"lodmixerSetTransform: D3DTS_PROJECTION. Matrix at %08x", (DWORD)pMatrix);
    }
}

void STDMETHODCALLTYPE lodmixerPresent(IDirect3DDevice9* self, CONST RECT* src, CONST RECT* dest,
	HWND hWnd, LPVOID unused)
{
    TRACE(L"lodmixerPresent: Present called.");
}

void initLodMixer()
{
    // skip the settings-check call
    TRACE(L"Initializing LOD mixer...");

    getConfig("lodmixer", "dx.screen.width", DT_DWORD, 1, lodmixerConfig);
    getConfig("lodmixer", "dx.screen.height", DT_DWORD, 2, lodmixerConfig);
    TRACE2N(L"Screen resolution to force: %dx%d", 
            _dxconfig.screen.width, _dxconfig.screen.height);

    BYTE* bptr = (BYTE*)code[C_SETTINGS_CHECK];
	DWORD protection;
    DWORD newProtection = PAGE_EXECUTE_READWRITE;
    if (VirtualProtect(bptr, 6, newProtection, &protection)) {
        /* CALL */
        bptr[0] = 0xe8;
        DWORD* ptr = (DWORD*)(code[C_SETTINGS_CHECK] + 1);
        ptr[0] = (DWORD)modSettings - (DWORD)(code[C_SETTINGS_CHECK] + 5);
        /* NOP */ 
        bptr[5] = 0x90;
        TRACE(L"Settings check disabled. Settings overwrite enabled.");
    } 

    hookFunction(hk_D3D_SetTransform, lodmixerSetTransform);
    //hookFunction(hk_D3D_Present, lodmixerPresent);

    TRACE(L"Initialization complete.");
}

void lodmixerConfig(char* pName, const void* pValue, DWORD a)
{
	switch (a) {
		case 1:	// width
			_dxconfig.screen.width = *(DWORD*)pValue;
			break;
		case 2: // height
			_dxconfig.screen.height = *(DWORD*)pValue;
			break;
	}
}

