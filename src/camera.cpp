/* <camera>
 *
 */
#define UNICODE
#define THISMOD &k_camera

#include <windows.h>
#include <stdio.h>
#include <sys/stat.h>
#include "kload_exp.h"
#include "camera.h"
#include "camera_addr.h"
#include "dllinit.h"
#include "configs.h"
#include "configs.hpp"
#include "utf8.h"

#define lang(s) getTransl("camera",s)

#include <map>
#include <list>
#include <hash_map>
#include <wchar.h>

#define SWAPBYTES(dw) \
    ((dw<<24 & 0xff000000) | (dw<<8  & 0x00ff0000) | \
    (dw>>8  & 0x0000ff00) | (dw>>24 & 0x000000ff))


// VARIABLES
HINSTANCE hInst = NULL;
KMOD k_camera = {MODID, NAMELONG, NAMESHORT, DEFAULT_DEBUG};

// GLOBALS

class config_t 
{
public:
    config_t() : angle(0) {}
    DWORD angle;
};

config_t _camera_config;

// FUNCTIONS
HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface);

void cameraConfig(char* pName, const void* pValue, DWORD a);

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
			k_camera.debug = *(DWORD*)pValue;
			break;
        case 2: // angle
			_camera_config.angle = *(DWORD*)pValue;
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

    getConfig("camera", "debug", DT_DWORD, 1, fservConfig);
    getConfig("camera", "angle", DT_DWORD, 2, fservConfig);

    LOG1N(L"configuration: angle = %d",_camera_config.angle);

    // patch code to load an arbitrary camera angle
    if (_camera_config.angle != 0)
    {
        DWORD points[] = {
            code[C_READ_CAMERA_ANGLE1], code[C_READ_CAMERA_ANGLE2],
        };

        int patchCount = 0;
        for (int i=0; i<2; i++)
        {
            BYTE* bptr = (BYTE*)points[i];
            DWORD protection = 0;
            DWORD newProtection = PAGE_EXECUTE_READWRITE;
            if (bptr && VirtualProtect(bptr, 16, newProtection, &protection)) 
            {
                bptr[0] = 0xb8;  // mov eax, _camera_config.angle
                DWORD* ptr = (DWORD*)(bptr + 1);
                ptr[0] = _camera_config.angle;
                // padding with NOP
                bptr[5] = 0x90;
                bptr[6] = 0x90;

                patchCount++;
            }
        }

        if (patchCount == 2)
            LOG1N(L"Camera angle set to: %d",_camera_config.angle);
    }
    else
        LOG1N(L"Using camera angle from SYSTEM DATA",_camera_config.angle);

	TRACE(L"Initialization complete.");
    return D3D_OK;
}

