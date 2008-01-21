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

wchar_t* g_afsFiles[] = {
    L"img\\cv_0.img",
    L"img\\cs.img",
    L"img\\rv_e.img",
    L"img\\rs_e.img",
    L"img\\cv_1.img",
    L"",
    L"img\\pinfo.img",
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

void kservBeforeProcessBinCallPoint();
void kservBeforeProcessBin(PACKED_BIN* bin, DWORD bufferSize);
DWORD kservGetBinSize(DWORD afsId, DWORD binId);
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

    MasterHookFunction(code[C_GET_BINSIZE], 2, kservGetBinSize);
    
    g_processBin = GetTargetAddress(code[C_PROCESS_BIN]);
    HookCallPoint(code[C_PROCESS_BIN], kservBeforeProcessBinCallPoint, 6, 0);

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

DWORD kservGetBinSize(DWORD afsId, DWORD binId)
{
    EnterCriticalSection(&g_csRead);
    DWORD threadId = GetCurrentThreadId();
    BIN_SIZE_INFO* pBinSizeInfo = ((BIN_SIZE_INFO**)data[BIN_SIZES_TABLE])[afsId];
    TRACE4N(L"kservGetBinSize:: thread %d: afsId=%d, binId=%d, size=%08x",
            threadId, afsId, binId, pBinSizeInfo->sizes[binId]);
    hash_map<DWORD,READ_BIN_STRUCT>::iterator it = g_read_bins.find(threadId);
    if (it == g_read_bins.end())
    {
        // insert new entry
        READ_BIN_STRUCT rbs;
        rbs.afsId = afsId;
        rbs.binId = binId;
        g_read_bins.insert(pair<DWORD,READ_BIN_STRUCT>(threadId,rbs));
    }
    else
    {
        // update entry (shouldn't happen, really)
        it->second.afsId = afsId; 
        it->second.binId = binId; 
    }
    LeaveCriticalSection(&g_csRead);

    // for BIN replacement:
    // This is the time-point #1, where you'll need to modify the binSizes table
    // IMPORTANT: NEVER DECREASE THE SIZE, ONLY INCREASE, if your bin is bigger,
    // otherwise, leave unmodified.
    // You will then be able to replace the content of unpacked bin, in kservBeforeProcessBin

    BIN_SIZE_INFO* pBST = ((BIN_SIZE_INFO**)data[BIN_SIZES_TABLE])[afsId];
    DWORD* bs = (DWORD*)&pBST->sizes;
    //...

    // call original
    DWORD result = MasterCallNext(afsId, binId);
    return result;
}

void kservBeforeProcessBinCallPoint()
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
        call kservBeforeProcessBin
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

void kservBeforeProcessBin(PACKED_BIN* bin, DWORD bufferSize)
{
    DWORD threadId = GetCurrentThreadId();
    EnterCriticalSection(&g_csRead);
    hash_map<DWORD,READ_BIN_STRUCT>::iterator bit = g_read_bins.find(threadId);
    if (bit != g_read_bins.end())
    {
        TRACE4N(L"kservBeforeProcessBin:: thread %d, afsId=%d, binId=%d, bin at %08x",
                bit->first,
                bit->second.afsId,
                bit->second.binId,
                (DWORD)bin
             );
        // for BIN replacement:
        // Here's the time-point #2, where you can replace the 
        // the contents of the BIN, which is yet to be processed. So, if you
        // modified the binSize in kservGetBinSize(), then you'll have enough
        // memory to replace the packed BIN content with your content.

        //...
    }
    LeaveCriticalSection(&g_csRead);
}

