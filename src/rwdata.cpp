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
};

config_t _rwdata_config;

// FUNCTIONS
HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface);

bool OpenFileIfExists(const wchar_t* filename, HANDLE& handle, DWORD& size);
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
        case 2: // edit.data
            _rwdata_config._edit_data_file = (wchar_t*)pValue;
            string_strip(_rwdata_config._edit_data_file);
            // verify existence and readability of the file;
            // expand relative name if needed.
            if (!OpenFileIfExists(_rwdata_config._edit_data_file.c_str(),
                        handle, fsize))
            {
                wstring fn(getPesInfo()->myDir);
                fn += L"\\data\\" + _rwdata_config._edit_data_file;
                if (OpenFileIfExists(fn.c_str(), handle, fsize))
                {
                    _rwdata_config._edit_data_file = fn;
                    CloseHandle(handle);
                }
                else
                {
                    LOG1S(L"ERROR: Unable to {%s} for reading. Will use default EDIT DATA file instead.",_rwdata_config._edit_data_file.c_str());
                    _rwdata_config._edit_data_file = L"";
                }
            }
            else
                CloseHandle(handle);
            break;
        case 3: // system.data
            _rwdata_config._system_data_file = (wchar_t*)pValue;
            string_strip(_rwdata_config._system_data_file);
            // verify existence and readability of the file;
            // expand relative name if needed.
            if (!OpenFileIfExists(_rwdata_config._system_data_file.c_str(),
                        handle, fsize))
            {
                wstring fn(getPesInfo()->myDir);
                fn += L"\\data\\" + _rwdata_config._system_data_file;
                if (OpenFileIfExists(fn.c_str(), handle, fsize))
                {
                    _rwdata_config._system_data_file = fn;
                    CloseHandle(handle);
                }
                else
                {
                    LOG1S(L"ERROR: Unable to {%s} for reading. Will use default SYSTEM DATA file instead.",_rwdata_config._system_data_file.c_str());
                    _rwdata_config._system_data_file = L"";
                }
            }
            else
                CloseHandle(handle);
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
    getConfig("rwdata", "edit.data", DT_STRING, 2, fservConfig);
    getConfig("rwdata", "system.data", DT_STRING, 3, fservConfig);

    LOG1N(L"debug = %d",k_rwdata.debug);
    LOG1S(L"edit.data = {%s}",_rwdata_config._edit_data_file.c_str());
    LOG1S(L"system.data = {%s}",_rwdata_config._system_data_file.c_str());

    _createFileW_r = (CREATEFILEW_PROC)HookIndirectCall(code[C_CREATEFILEW_R],
            rwdataCreateFileW_r);
    _createFileW_w = (CREATEFILEW_PROC)HookIndirectCall(code[C_CREATEFILEW_W],
            rwdataCreateFileW_w);

	TRACE(L"Hooking done.");
    return D3D_OK;
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
    int len = wcslen(lpFileName);
    int elen = wcslen(L"EDIT01.bin");
    int slen = wcslen(L"OPTION01.bin");
    if (wcsncmp(lpFileName+len-elen, L"EDIT01.bin", elen)==0 &&
            !_rwdata_config._edit_data_file.empty())
        lpFileName = _rwdata_config._edit_data_file.c_str();
    else if (wcsncmp(lpFileName+len-slen, L"OPTION01.bin", slen)==0 &&
            !_rwdata_config._system_data_file.empty())
        lpFileName = _rwdata_config._system_data_file.c_str();

    if (k_rwdata.debug)
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
    int len = wcslen(lpFileName);
    int elen = wcslen(L"EDIT01.bin");
    int slen = wcslen(L"OPTION01.bin");
    if (wcsncmp(lpFileName+len-elen, L"EDIT01.bin", elen)==0 &&
            !_rwdata_config._edit_data_file.empty())
        lpFileName = _rwdata_config._edit_data_file.c_str();
    else if (wcsncmp(lpFileName+len-slen, L"OPTION01.bin", slen)==0 &&
            !_rwdata_config._system_data_file.empty())
        lpFileName = _rwdata_config._system_data_file.c_str();

    if (k_rwdata.debug)
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

bool OpenFileIfExists(const wchar_t* filename, HANDLE& handle, DWORD& size)
{
    TRACE1S(L"OpenFileIfExists:: %s", filename);
    handle = CreateFile(filename,             // file to open
                       GENERIC_READ | GENERIC_WRITE, // open for read/write
                       0,                     // no sharing
                       NULL,                  // default security
                       OPEN_EXISTING,         // existing file only
                       FILE_ATTRIBUTE_NORMAL, // normal file
                       NULL);                 // no attr. template

    if (handle != INVALID_HANDLE_VALUE)
    {
        size = GetFileSize(handle,NULL);
        return true;
    }
    return false;
}

