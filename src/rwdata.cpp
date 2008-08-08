/* <rwdata>
 *
 */
#define UNICODE
#define THISMOD &k_rwdata

#include <windows.h>
#include <stdio.h>
#include <sys/stat.h>
#include "kload_exp.h"
#include "rwdata.h"
#include "rwdata_addr.h"
#include "dllinit.h"
#include "configs.h"
#include "configs.hpp"
#include "utf8.h"

#define lang(s) getTransl("rwdata",s)

#include <map>
#include <list>
#include <hash_map>
#include <wchar.h>

#define SWAPBYTES(dw) \
    ((dw<<24 & 0xff000000) | (dw<<8  & 0x00ff0000) | \
    (dw>>8  & 0x0000ff00) | (dw>>24 & 0x000000ff))


// VARIABLES
HINSTANCE hInst = NULL;
KMOD k_rwdata = {MODID, NAMELONG, NAMESHORT, DEFAULT_DEBUG};

// GLOBALS
typedef HANDLE (WINAPI *CREATEFILEW_PROC)(
  LPCTSTR lpFileName,
  DWORD dwDesiredAccess,
  DWORD dwShareMode,
  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  DWORD dwCreationDisposition,
  DWORD dwFlagsAndAttributes,
  HANDLE hTemplateFile
);

CREATEFILEW_PROC _createFileW_r = NULL;
CREATEFILEW_PROC _createFileW_w = NULL;

class config_t 
{
public:
    wstring _edit_data_file;
    wstring _system_data_file;
    wstring _data_dir;
};

config_t _rwdata_config;
char _save_dir[1024];

// FUNCTIONS
HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface);

DWORD HookIndirectCall(DWORD addr, void* func);
void rwdataConfig(char* pName, const void* pValue, DWORD a);
HANDLE WINAPI rwdataCreateFileW_r(
  LPCTSTR lpFileName,
  DWORD dwDesiredAccess,
  DWORD dwShareMode,
  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  DWORD dwCreationDisposition,
  DWORD dwFlagsAndAttributes,
  HANDLE hTemplateFile);
HANDLE WINAPI rwdataCreateFileW_w(
  LPCTSTR lpFileName,
  DWORD dwDesiredAccess,
  DWORD dwShareMode,
  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  DWORD dwCreationDisposition,
  DWORD dwFlagsAndAttributes,
  HANDLE hTemplateFile);

void rwdataCalculateStringSize();

static void string_strip(wstring& s)
{
    static const wchar_t* empties = L" \t\n\r";
    int e = s.find_last_not_of(empties);
    s.erase(e + 1);
    int b = s.find_first_not_of(empties);
    s.erase(0,b);
}

static void string_strip_quotes(wstring& s)
{
    static const wchar_t* empties = L" \t\n\r";
    if (s[s.length()-1]=='"')
        s.erase(s.length()-1);
    if (s[0]=='"')
        s.erase(0,1);
}


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
			k_rwdata.debug = *(DWORD*)pValue;
			break;
        case 2: // data.dir
            _rwdata_config._data_dir = (wchar_t*)pValue;
            string_strip(_rwdata_config._data_dir);
            if (!_rwdata_config._data_dir.empty())
                string_strip_quotes(_rwdata_config._data_dir);
            break;
	}
	return;
}

HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface) {

	unhookFunction(hk_D3D_CreateDevice, initModule);

    LOG(L"Initializing Read-Write Data Module");

    getConfig("rwdata", "debug", DT_DWORD, 1, fservConfig);
    getConfig("rwdata", "data.dir", DT_STRING, 2, fservConfig);

    LOG1N(L"debug = %d",k_rwdata.debug);
    LOG1S(L"data.dir = {%s}",_rwdata_config._data_dir.c_str());

    if (k_rwdata.debug)
    {
        // just for tracking read/writes
        _createFileW_r = (CREATEFILEW_PROC)HookIndirectCall(
                code[C_CREATEFILEW_R],
                rwdataCreateFileW_r);
        _createFileW_w = (CREATEFILEW_PROC)HookIndirectCall(
                code[C_CREATEFILEW_W],
                rwdataCreateFileW_w);
    }

    HookCallPoint(code[C_CALC_STRING_SIZE], rwdataCalculateStringSize, 6, 0);
    if (!_rwdata_config._data_dir.empty())
    {
        ZeroMemory(_save_dir, sizeof(_save_dir));
        Utf8::fUnicodeToAnsi(_save_dir,_rwdata_config._data_dir.c_str());

	    DWORD protection = 0;
	    DWORD newProtection = PAGE_EXECUTE_READWRITE;
        DWORD* dptr = (DWORD*)code[C_SET_DIRNAME];
        if (VirtualProtect(dptr, 4, newProtection, &protection)) {
            *dptr = (DWORD)_save_dir;
            LOG1S(L"Save dir set to: %s", _rwdata_config._data_dir.c_str());
        }
    }

	TRACE(L"Hooking done.");
    return D3D_OK;
}

void rwdataCalculateStringSize()
{
    __asm {
        sub eax,edx  // execute replaced code
        mov ebx,8192 // big enough buffer
        retn
    }
}

DWORD HookIndirectCall(DWORD addr, void* func)
{
    DWORD target = (DWORD)func;
	if (addr && target)
	{
	    BYTE* bptr = (BYTE*)addr;
        DWORD* orgTarget = *(DWORD**)(bptr + 2);
	    DWORD protection = 0;
	    DWORD newProtection = PAGE_EXECUTE_READWRITE;
	    if (VirtualProtect(bptr, 16, newProtection, &protection)) {
	        bptr[0] = 0xe8;
	        DWORD* ptr = (DWORD*)(addr + 1);
	        ptr[0] = target - (DWORD)(addr + 5);
            // padding with NOP
            bptr[5] = 0x90;
	        TRACE2N(L"Function (%08x) HOOKED at address (%08x)", target, addr);
            return *orgTarget;
	    }
	}
    return NULL;
}

HANDLE WINAPI rwdataCreateFileW_r(
  LPCTSTR lpFileName,
  DWORD dwDesiredAccess,
  DWORD dwShareMode,
  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  DWORD dwCreationDisposition,
  DWORD dwFlagsAndAttributes,
  HANDLE hTemplateFile
)
{
    LOG1S(L"CreateFileW (READ) called for {%s}",lpFileName);

    // call original
    HANDLE result = _createFileW_r(
            lpFileName,
            dwDesiredAccess,
            dwShareMode,
            lpSecurityAttributes,
            dwCreationDisposition,
            dwFlagsAndAttributes,
            hTemplateFile);

    return result;
}

HANDLE WINAPI rwdataCreateFileW_w(
  LPCTSTR lpFileName,
  DWORD dwDesiredAccess,
  DWORD dwShareMode,
  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  DWORD dwCreationDisposition,
  DWORD dwFlagsAndAttributes,
  HANDLE hTemplateFile
)
{
    LOG1S(L"CreateFileW (WRITE) called for {%s}",lpFileName);

    // call original
    HANDLE result = _createFileW_w(
            lpFileName,
            dwDesiredAccess,
            dwShareMode,
            lpSecurityAttributes,
            dwCreationDisposition,
            dwFlagsAndAttributes,
            hTemplateFile);

    return result;
}

