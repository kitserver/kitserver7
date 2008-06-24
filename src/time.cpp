/* <time>
 *
 */
#define UNICODE
#define THISMOD &k_time

#include <windows.h>
#include <stdio.h>
#include <sys/stat.h>
#include "kload_exp.h"
#include "time.h"
#include "time_addr.h"
#include "dllinit.h"
#include "configs.h"
#include "configs.hpp"
#include "utf8.h"

#define lang(s) getTransl("time",s)

#include <map>
#include <list>
#include <hash_map>
#include <wchar.h>

#define SWAPBYTES(dw) \
    ((dw<<24 & 0xff000000) | (dw<<8  & 0x00ff0000) | \
    (dw>>8  & 0x0000ff00) | (dw>>24 & 0x000000ff))


// VARIABLES
HINSTANCE hInst = NULL;
KMOD k_time = {MODID, NAMELONG, NAMESHORT, DEFAULT_DEBUG};

// GLOBALS

class config_t 
{
public:
    config_t() : exhib_time(0), cup_time(0), fatigue_factor(1.0) {}
    BYTE exhib_time;
    BYTE cup_time;
    float fatigue_factor;
};

config_t _time_config;

// FUNCTIONS
HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface);

void timeConfig(char* pName, const void* pValue, DWORD a);

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

void fservConfig(char* pName, const void* pValue, DWORD a)
{
    HANDLE handle;
    DWORD fsize;

	switch (a) {
		case 1: // debug
			k_time.debug = *(DWORD*)pValue;
			break;
        case 2: // exhibition time
			_time_config.exhib_time = *(DWORD*)pValue;
            break;
        case 3: // cup time
			_time_config.cup_time = *(DWORD*)pValue;
            break;
        case 4: // fatigue factor
			_time_config.fatigue_factor = *(float*)pValue;
            break;
	}
	return;
}

HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface) {

	unhookFunction(hk_D3D_CreateDevice, initModule);

    LOG(L"Initializing Camera Module");

    getConfig("time", "debug", DT_DWORD, 1, fservConfig);
    getConfig("time", "exhibition.time", DT_DWORD, 2, fservConfig);
    getConfig("time", "cup.time", DT_DWORD, 3, fservConfig);
    getConfig("time", "fatigue.factor", DT_FLOAT, 4, fservConfig);

    LOG1N(L"configuration: exhibition.time = %d",_time_config.exhib_time);
    LOG1N(L"configuration: cup.time = %d",_time_config.cup_time);
    LOG1F(L"configuration: fatigue.factor = %0.4f",_time_config.fatigue_factor);

    // patch code to enforce exhibition time
    if (_time_config.exhib_time != 0)
    {
        BYTE* bptr = (BYTE*)code[C_SET_EXHIB_TIME];
        DWORD protection = 0;
        DWORD newProtection = PAGE_EXECUTE_READWRITE;
        if (bptr && VirtualProtect(bptr, 8, newProtection, &protection)) 
        {
            bptr[0] = 0xb0;  // mov al, _time_config.exhib_time
            bptr[1] = _time_config.exhib_time;
            // padding with NOPs
            bptr[2] = 0x90;
            bptr[3] = 0x90;
            bptr[4] = 0x90;
            bptr[5] = 0x90;
        }
    }

    // patch code to enforce cup time
    if (_time_config.cup_time != 0)
    {
        BYTE* bptr = (BYTE*)code[C_SET_CUP_TIME];
        DWORD protection = 0;
        DWORD newProtection = PAGE_EXECUTE_READWRITE;
        if (bptr && VirtualProtect(bptr, 8, newProtection, &protection)) 
        {
            bptr[0] = 0xb9;  // mov ecx, _time_config.exhib_time
            bptr[1] = _time_config.cup_time;
            bptr[2] = 0;
            bptr[3] = 0;
            bptr[4] = 0;
            bptr[5] = 0xeb; // jmp short <to_after>
            bptr[6] = 0x26;
            // padding with NOPs
            bptr[7] = 0x90;
            bptr[8] = 0x90;
            bptr[9] = 0x90;
            bptr[10] = 0x90;
            bptr[11] = 0x90;
        }

        BYTE enduranceValue = 
            _time_config.cup_time * _time_config.fatigue_factor;

        bptr = (BYTE*)code[C_SET_CUP_ENDURANCE];
        protection = 0;
        newProtection = PAGE_EXECUTE_READWRITE;
        if (bptr && VirtualProtect(bptr, 8, newProtection, &protection)) 
        {
            bptr[0] = 0xc7;  // mov ecx, enduranceValue
            bptr[1] = 0x44;
            bptr[2] = 0x24;
            bptr[3] = 0x24;
            bptr[4] = enduranceValue;
            bptr[5] = 0;
            bptr[6] = 0;
            bptr[7] = 0;
            // padding with NOPs
            for (int i=8; i<20; i++)
                bptr[i] = 0x90;
        }
    }

	TRACE(L"Initialization complete.");
    return D3D_OK;
}

