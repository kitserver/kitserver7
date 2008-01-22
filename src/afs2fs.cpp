/* AFS2FS module */
#define UNICODE
#define THISMOD &k_kafs

#include <windows.h>
#include <stdio.h>
#include "kload_exp.h"
#include "afs2fs.h"
#include "afs2fs_addr.h"
#include "dllinit.h"
#include "gdb.h"
#include "pngdib.h"
#include "utf8.h"

#define lang(s) getTransl("afs2fs",s)

#include <map>
#include <hash_map>
#include <wchar.h>

#define SWAPBYTES(dw) \
    ((dw<<24 & 0xff000000) | (dw<<8  & 0x00ff0000) | \
    (dw>>8  & 0x0000ff00) | (dw>>24 & 0x000000ff))

wchar_t* g_afsDirs[] = {
    L"img\\cv_0",
    L"img\\cs",
    L"img\\rv_e",
    L"img\\rs_e",
    L"img\\cv_1",
    L"",
    L"img\\pinfo",
};

// VARIABLES
HINSTANCE hInst = NULL;
KMOD k_kafs = {MODID, NAMELONG, NAMESHORT, DEFAULT_DEBUG};

// GLOBALS
CRITICAL_SECTION g_csRead;
hash_map<DWORD,READ_BIN_STRUCT> g_read_bins;
DWORD g_processBin = 0;

// FUNCTIONS
HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface);

void afsBeforeProcessBinCallPoint();
void afsAfterGetBinBufferSizeCallPoint();
KEXPORT void afsBeforeProcessBin(PACKED_BIN* bin, DWORD bufferSize);
KEXPORT DWORD afsAfterGetBinBufferSize(GET_BIN_SIZE_STRUCT* gbss, DWORD orgSize);
DWORD GetTargetAddress(DWORD addr);
void HookCallPoint(DWORD addr, void* func, int codeShift, int numNops);

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

        // initialize critical sections
        InitializeCriticalSection(&g_csRead);

		copyAdresses();
		hookFunction(hk_D3D_CreateDevice, initModule);
	}
	
	else if (dwReason == DLL_PROCESS_DETACH)
	{
        // destroy critical sections
        DeleteCriticalSection(&g_csRead);
	}
	
	return true;
}

HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface) {
	
	unhookFunction(hk_D3D_CreateDevice, initModule);

    HookCallPoint(code[C_AFTER_GET_BINBUFFERSIZE], afsAfterGetBinBufferSizeCallPoint, 6, 1);
    
    g_processBin = GetTargetAddress(code[C_PROCESS_BIN]);
    HookCallPoint(code[C_PROCESS_BIN], afsBeforeProcessBinCallPoint, 6, 0);

	TRACE(L"Hooking done.");
    return D3D_OK;
}

void HookCallPoint(DWORD addr, void* func, int codeShift, int numNops)
{
    DWORD target = (DWORD)func + codeShift;
	if (addr && target)
	{
	    BYTE* bptr = (BYTE*)addr;
	    DWORD protection = 0;
	    DWORD newProtection = PAGE_EXECUTE_READWRITE;
	    if (VirtualProtect(bptr, 16, newProtection, &protection)) {
	        bptr[0] = 0xe8;
	        DWORD* ptr = (DWORD*)(addr + 1);
	        ptr[0] = target - (DWORD)(addr + 5);
            // padding with NOPs
            for (int i=0; i<numNops; i++) bptr[5+i] = 0x90;
	        TRACE2N(L"Function (%08x) HOOKED at address (%08x)", target, addr);
	    }
	}
}

DWORD GetTargetAddress(DWORD addr)
{
	if (addr)
	{
	    BYTE* bptr = (BYTE*)addr;
	    DWORD protection = 0;
	    DWORD newProtection = PAGE_EXECUTE_READWRITE;
	    if (VirtualProtect(bptr, 8, newProtection, &protection)) 
        {
            // get original target
            DWORD* ptr = (DWORD*)(addr + 1);
            return (DWORD)(ptr[0] + addr + 5);
	    }
	}
    return 0;
}

/**
 * Simple file-check routine.
 */
bool OpenFileIfExists(const wchar_t* filename, HANDLE& handle, DWORD& size)
{
    TRACE1S(L"FileExists:: Checking file: %s", filename);
    handle = CreateFile(filename,           // file to open
                       GENERIC_READ,          // open for reading
                       FILE_SHARE_READ,       // share for reading
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

void afsAfterGetBinBufferSizeCallPoint()
{
    __asm {
        // IMPORTANT: when saving flags, use pusfd/popfd, because Windows
        // apparently checks for stack alignment and bad things happen, if it's not
        // DWORD-aligned. (For example, all file operations fail!)
        pushfd 
        push ebp
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push eax // org-size
        push ecx 
        call afsAfterGetBinBufferSize
        add esp,8     // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop ebp
        popfd
        mov edi,eax // execute replaced code
        mov ecx,edi
        or ecx,edx
        retn
    }
}

KEXPORT DWORD afsAfterGetBinBufferSize(GET_BIN_SIZE_STRUCT* gbss, DWORD orgSize)
{
    DWORD result = orgSize;

    // check for a file
    wchar_t filename[1024] = {0};
    if (gbss->afsId < 7)
        swprintf(filename,L"%s\\%s\\unnamed_%d.bin",
                getPesInfo()->myDir,
                g_afsDirs[gbss->afsId],
                gbss->binId);

    HANDLE hfile;
    DWORD fsize = 0;
    if (OpenFileIfExists(filename, hfile, fsize))
    {
        EnterCriticalSection(&g_csRead);
        DWORD threadId = GetCurrentThreadId();
        hash_map<DWORD,READ_BIN_STRUCT>::iterator it = g_read_bins.find(threadId);
        if (it == g_read_bins.end())
        {
            // insert new entry
            READ_BIN_STRUCT rbs;
            rbs.afsId = gbss->afsId;
            rbs.binId = gbss->binId;
            rbs.hfile = hfile;
            rbs.fsize = fsize;
            g_read_bins.insert(pair<DWORD,READ_BIN_STRUCT>(threadId,rbs));
        }
        else
        {
            // update entry (shouldn't happen, really)
            it->second.afsId = gbss->afsId; 
            it->second.binId = gbss->binId; 

            CloseHandle(it->second.hfile);
            it->second.hfile = hfile;
        }
        LeaveCriticalSection(&g_csRead);

        // modify BIN buffer size
        // IMPORTANT: don't decrease: only increase.
        DWORD bytesRead(0);
        PACKED_BIN_HEADER hdr;
        ReadFile(hfile, &hdr, sizeof(PACKED_BIN_HEADER), &bytesRead, 0);
        DWORD newSize = hdr.sizePacked + sizeof(PACKED_BIN_HEADER) + (0x800 - hdr.sizePacked % 0x800);
        result = max(orgSize, newSize);
    }
    return result;
}

void afsBeforeProcessBinCallPoint()
{
    __asm {
        // IMPORTANT: when saving flags, use pusfd/popfd, because Windows
        // apparently checks for stack alignment and bad things happen, if it's not
        // DWORD-aligned. (For example, all file operations fail!)
        pushfd 
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        mov ecx,[esp+0x24]
        mov edx,[esp+0x28]
        push edx
        push ecx
        call afsBeforeProcessBin
        add esp,8     // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        jmp g_processBin // execute replaced code
    }
}

KEXPORT void afsBeforeProcessBin(PACKED_BIN* bin, DWORD bufferSize)
{
    DWORD threadId = GetCurrentThreadId();
    EnterCriticalSection(&g_csRead);
    hash_map<DWORD,READ_BIN_STRUCT>::iterator bit = g_read_bins.find(threadId);
    if (bit != g_read_bins.end())
    {
        TRACE4N(L"afsBeforeProcessBin:: thread %d, afsId=%d, binId=%d, bin at %08x",
                bit->first,
                bit->second.afsId,
                bit->second.binId,
                (DWORD)bin
             );
        // for BIN replacement:
        // Here's the time-point #2, where you can replace the 
        // the contents of the BIN, which is yet to be processed. So, if you
        // modified the bin-size in afsAfterGetBinBufferSize(), then you'll have enough
        // memory to replace the packed BIN content with your content.
        DWORD bytesRead(0);
        SetFilePointer(bit->second.hfile, 0, NULL, FILE_BEGIN);
        if (ReadFile(bit->second.hfile, bin, bit->second.fsize, &bytesRead, 0))
        {
            LOG2N(L"Loaded BIN for afsId=%d, binId=%d.", 
                    bit->second.afsId, bit->second.binId);
        }
        else
        {
            LOG3N(L"ERROR loading file for afsId=%d, binId=%d. Error=%d",
                    bit->second.afsId,
                    bit->second.binId,
                    GetLastError());
        }
        CloseHandle(bit->second.hfile);
        g_read_bins.erase(bit);
    }
    LeaveCriticalSection(&g_csRead);
}

