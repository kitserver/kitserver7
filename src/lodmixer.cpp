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
LMCONFIG _lmconfig = {
    {DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_ASPECT_RATIO},
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
    // set resolution
    if (_lmconfig.screen.width>0 && _lmconfig.screen.height>0)
    {
        *(DWORD*)data[SCREEN_WIDTH] = _lmconfig.screen.width;
        *(DWORD*)data[SCREEN_HEIGHT] = _lmconfig.screen.height;

        LOG2N(L"Resolution set: %dx%d", _lmconfig.screen.width, _lmconfig.screen.height);
    }

    // set aspect ratio
    float ar = (_lmconfig.screen.aspectRatio > 0.01) ? 
        _lmconfig.screen.aspectRatio :   // manual
        ((float)_lmconfig.screen.width / (float)_lmconfig.screen.height); // automatic

    if (fabs(ar - 1.33333) < fabs(ar - 1.77777)) {
        // closer to 4:3
        *(DWORD*)data[WIDESCREEN_FLAG] = 0;
        *(float*)data[RATIO_4on3] = ar;
        LOG(L"Widescreen mode: no");
    } else {
        // closer to 16:9
        *(DWORD*)data[WIDESCREEN_FLAG] = 1;
        *(float*)data[RATIO_16on9] = ar;
        LOG(L"Widescreen mode: yes");
    }
    LOG1F(L"Aspect ratio: %0.5f", ar);
    LOG1S(L"Aspect ratio type: %s", 
            (_lmconfig.screen.aspectRatio > 0.01)?L"manual":L"automatic");
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

    getConfig("lodmixer", "screen.width", DT_DWORD, 1, lodmixerConfig);
    getConfig("lodmixer", "screen.height", DT_DWORD, 2, lodmixerConfig);
    getConfig("lodmixer", "screen.aspect-ratio", DT_FLOAT, 3, lodmixerConfig);
    TRACE2N(L"Screen resolution to force: %dx%d", 
            _lmconfig.screen.width, _lmconfig.screen.height);

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

    TRACE(L"Initialization complete.");
}

void lodmixerConfig(char* pName, const void* pValue, DWORD a)
{
	switch (a) {
		case 1:	// width
			_lmconfig.screen.width = *(DWORD*)pValue;
			break;
		case 2: // height
			_lmconfig.screen.height = *(DWORD*)pValue;
			break;
		case 3: // aspect ratio
			_lmconfig.screen.aspectRatio = *(float*)pValue;
			break;
	}
}

