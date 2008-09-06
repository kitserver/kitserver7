/* Kitserver module */
#define UNICODE
#define THISMOD &k_kserv

#include <windows.h>
#include <stdio.h>
#include "kload_exp.h"
#include "kserv.h"
#include "kserv_addr.h"
#include "dllinit.h"
#include "gdb.h"
#include "pngdib.h"
#include "utf8.h"
#include "commctrl.h"
#include "afsio.h"

#if _CPPLIB_VER < 503
#define  __in
#define  __out 
#define  __in
#define  __out_opt
#define  __inout_opt
#endif

#define lang(s) getTransl("kserv",s)

#include <map>
#include <hash_map>
#include <wchar.h>

#define SWAPBYTES(dw) \
    ((dw<<24 & 0xff000000) | (dw<<8  & 0x00ff0000) | \
    (dw>>8  & 0x0000ff00) | (dw>>24 & 0x000000ff))

#define COLOR_BLACK D3DCOLOR_RGBA(0,0,0,128)
#define COLOR_AUTO D3DCOLOR_RGBA(135,135,135,255)
#define COLOR_CHOSEN D3DCOLOR_RGBA(210,210,210,255)
#define COLOR_INFO D3DCOLOR_RGBA(0xb0,0xff,0xb0,0xff)

#define GLOVES_BIN 5703

typedef struct _UNPACK_INFO 
{
    BYTE* srcBuffer;
    DWORD bytesToProcess;
    DWORD bytesProcessed;
    BYTE* destBuffer;
    DWORD destSizeExpected;
    DWORD destSize;
} UNPACK_INFO;

typedef DWORD (*PROC_UNPACK_BIN)(UNPACK_INFO* pUnpackInfo, DWORD p2);
PROC_UNPACK_BIN _orgUnpackBin;

typedef struct _SWAP_BIN_STRUCT
{
    PACKED_BIN* swap_bin;
    BYTE* srcBuffer;
    DWORD bytesToProcess;
} SWAP_BIN_STRUCT;

typedef struct _READ_BIN_STRUCT
{
    DWORD afsId;
    DWORD binId;
} READ_BIN_STRUCT;

typedef struct _LOAD_BIN_STRUCT
{
    BYTE unknown1[0x0c];
    DWORD afsId;
    DWORD binId;
} LOAD_BIN_STRUCT;

enum
{
    BIN_KIT_GK,
    BIN_KIT_PL,
    BIN_FONT_GA,
    BIN_FONT_GB,
    BIN_FONT_PA,
    BIN_FONT_PB,
    BIN_NUMS_GA,
    BIN_NUMS_GB,
    BIN_NUMS_PA,
    BIN_NUMS_PB,
};

class kserv_config_t 
{
public:
    kserv_config_t() : _use_description(true) {}
    bool _use_description;
};

// VARIABLES
HINSTANCE hInst = NULL;
KMOD k_kserv = {MODID, NAMELONG, NAMESHORT, DEFAULT_DEBUG};
bool allQualities = true;

#define TOTAL_TEAMS 288
#define NUM_REGULAR_TEAMS 220
#define NUM_ADDITIONAL_TEAMS 272
#define NUM_SLOTS 256
#define FIRST_KIT_BIN  7523
#define FIRST_FONT_BIN 2401
#define FIRST_NUMS_BIN 3425

#define PDATA_IMG 6
#define SOUTH_KOREA_GK 5
#define SOUTH_KOREA_PL 6
#define SOUTH_KOREA_GK_CV0 7547 
#define SOUTH_KOREA_PL_CV0 7548

// GLOBALS
CRITICAL_SECTION g_csRead;
CRITICAL_SECTION _cs_savedTki;
CRITICAL_SECTION _cs_slots;

kserv_config_t _kserv_config;

HANDLE g_hfile_cv_0 = INVALID_HANDLE_VALUE;
FILE *g_FILE_cv_0 = NULL;

hash_map<DWORD,BIN_INFO> g_kitsBinInfoById;
hash_map<DWORD,BIN_INFO> g_kitsBinInfoByOffset;
hash_map<DWORD,SWAP_BIN_STRUCT> g_swap_bins;
hash_map<DWORD,READ_BIN_STRUCT> g_read_bins;

DWORD g_currentBin = 0xffffffff;
hash_map<WORD,WORD> g_teamIdByKitSlot; // to lookup team ID, using kit slot
hash_map<WORD,WORD> g_kitSlotByTeamId; // to lookup kit slot, using team ID
DWORD g_teamKitInfoBase = 0;
bool g_slotMapsInitialized = false;
bool g_presentHooked = false;
bool g_beginShowKitSelection = false;
DWORD g_cupModeInd = 0;
hash_map<DWORD,TEAM_KIT_INFO> g_savedTki;

WORD g_lastHome = 0xffff;
WORD g_lastAway = 0xffff;
bool _home_forced = false;
bool _away_forced = false;

GDB* gdb = NULL;
DWORD g_return_addr = 0;
DWORD g_param = 0;

// kit iterators
map<wstring,Kit>::iterator g_iterHomePL;
map<wstring,Kit>::iterator g_iterAwayPL;
map<wstring,Kit>::iterator g_iterHomeGK;
map<wstring,Kit>::iterator g_iterAwayGK;

map<wstring,Kit>::iterator g_iterHomePL_begin;
map<wstring,Kit>::iterator g_iterAwayPL_begin;
map<wstring,Kit>::iterator g_iterHomeGK_begin;
map<wstring,Kit>::iterator g_iterAwayGK_begin;
map<wstring,Kit>::iterator g_iterHomePL_end;
map<wstring,Kit>::iterator g_iterAwayPL_end;
map<wstring,Kit>::iterator g_iterHomeGK_end;
map<wstring,Kit>::iterator g_iterAwayGK_end;

static int _myPage = -1;

// FUNCTIONS
HRESULT STDMETHODCALLTYPE initKserv(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface);
void kservConfig(char* pName, const void* pValue, DWORD a);
void setQualityChecks();
void kservRenderPlayer(TexPlayerInfo* tpi, DWORD coll, DWORD num, WORD* orgTexIds, BYTE orgTexMaxNum);
BOOL WINAPI kservSetFilePointerEx(
  __in       HANDLE hFile,
  __in       LARGE_INTEGER liDistanceToMove,
  __out_opt  PLARGE_INTEGER lpNewFilePointer,
  __in       DWORD dwMoveMethod
);
BOOL WINAPI kservReadFile(
  __in         HANDLE hFile,
  __out        LPVOID lpBuffer,
  __in         DWORD nNumberOfBytesToRead,
  __out_opt    LPDWORD lpNumberOfBytesRead,
  __inout_opt  LPOVERLAPPED lpOverlapped
);
void kservBeforeLoadBinCallPoint();
void kservBeforeLoadBinCallPoint_120();
KEXPORT void kservBeforeLoadBin(LOAD_BIN_STRUCT* s1, LOAD_BIN_STRUCT* s2);
void kservUnpackBinCallPoint();
KEXPORT DWORD kservUnpackBin(UNPACK_INFO* pUnpackInfo, DWORD p2);
DWORD kservGetBinSize(DWORD afsId, DWORD binId);
void HookAt(DWORD addr, void* func);
PACKED_BIN* LoadBinFromAFS(DWORD id);
KEXPORT bool LoadBinFromAFS2(DWORD afsId, DWORD binId, PACKED_BIN* bin);
void DumpData(void* data, size_t size);
DWORD LoadPNGTexture(BITMAPINFO** tex, const wchar_t* filename);
void ApplyAlphaChunk(RGBQUAD* palette, BYTE* memblk, DWORD size);
static int read_file_to_mem(const wchar_t *fn,unsigned char **ppfiledata, int *pfilesize);
void ApplyDIBTexture(TEXTURE_ENTRY* tex, BITMAPINFO* bitmap);
void FreePNGTexture(BITMAPINFO* bitmap);
void ReplaceTexturesInBin(UNPACKED_BIN* bin, wstring files[], size_t n);
WORD GetTeamIdBySrc(TEAM_KIT_INFO* src);
TEAM_KIT_INFO* GetTeamKitInfoById(WORD id);
KEXPORT void kservAfterReadTeamKitInfo(TEAM_KIT_INFO* src, TEAM_KIT_INFO* dest);
void kservReadTeamKitInfoCallPoint1();
void kservReadTeamKitInfoCallPoint2();
void kservReadTeamKitInfoCallPoint3();
void InitSlotMap();
int GetBinType(DWORD id);
int GetKitSlot(DWORD id);
WORD FindFreeKitSlot();
void ApplyKitAttributes(map<wstring,Kit>& m, const wchar_t* kitKey, KIT_INFO& ki);
KEXPORT void ApplyKitAttributes(const map<wstring,Kit>::iterator kiter, KIT_INFO& ki);
void RGBAColor2KCOLOR(const RGBAColor& color, KCOLOR& kcolor);
void KCOLOR2RGBAColor(const KCOLOR kcolor, RGBAColor& color);
void kservPresent(IDirect3DDevice9* self, CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused);
void ResetIterators();
void kservStartReadKitInfoCallPoint();
void kservStartReadKitInfoCallPoint2();
void kservStartReadKitInfoCallPoint3();
void kservEndReadKitInfoCallPoint();
void kservStartReadKitInfo(TEAM_KIT_INFO* src);
void kservEndReadKitInfo();
void SetAttributes(TEAM_KIT_INFO* src, WORD teamId);
void RestoreAttributes(TEAM_KIT_INFO* src);
void kservOnEnterCupsCallPoint();
void kservOnLeaveCupsCallPoint();
void kservOnLeaveCupsCallPoint2();
void kservOnEnterCups();
void kservOnLeaveCups();
void kservAtLoadHomeKeeperCallPoint();
void kservAtLoadAwayKeeperCallPoint();
KEXPORT DWORD kservAtLoadHomeKeeper(DWORD teamId);
KEXPORT DWORD kservAtLoadAwayKeeper(DWORD teamId);
//void kservAtWriteTeamIdCallPoint();
//KEXPORT void kservAtWriteTeamId(TEAM_MATCH_DATA_INFO*);
bool IsKitBin(DWORD afsId, DWORD binId);
void kservOverlayEvent(bool overlayOn, bool isExhibitionMode, int delta, DWORD menuMode);
void kservKeyboardEvent(int code1, WPARAM wParam, LPARAM lParam);
void kservReadReplayData(LPCVOID data, DWORD size);
void kservWriteReplayData(LPCVOID data, DWORD size);
bool kservGetFileInfo(DWORD afsId, DWORD binId, HANDLE& hfile, DWORD& fsize);
void kservAtReadKitSelectionCallPoint();
void kservAtReadKitSelectionCallPoint2();
KEXPORT void kservAtReadKitSelection(TEAM_MATCH_DATA_INFO* tmdi);
void kservMenuEvent(int delta, DWORD menuMode, DWORD ind, DWORD inGameInd, DWORD cupModeInd);
void kservPtrCheckCallPoint();
KEXPORT void kservPtrCheck(DWORD** pptr);

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

        CHECK_KLOAD(MAKELONG(3,7));

        // we need to do this early so that the overlay pages
        // appear in the same order as DLLs in config.txt
        _myPage = addOverlayCallback(kservOverlayEvent,true);
        LOG1N(L"_myPage = %d", _myPage);

        // initialize critical sections
        InitializeCriticalSection(&g_csRead);
        InitializeCriticalSection(&_cs_savedTki);
        InitializeCriticalSection(&_cs_slots);

		copyAdresses();
		//hookFunction(hk_D3D_Create, initKserv);
		hookFunction(hk_D3D_Create, setQualityChecks);
		hookFunction(hk_D3D_CreateDevice, initKserv);
	}
	
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		LOG(L"Shutting down this module...");
        delete gdb;
		unhookFunction(hk_D3D_Present, kservPresent);

        #ifndef MYDLL_RELEASE_BUILD
        unhookFunction(hk_RenderPlayer, kservRenderPlayer);
        #endif

        if (g_hfile_cv_0 != INVALID_HANDLE_VALUE)
            CloseHandle(g_hfile_cv_0);
        if (g_FILE_cv_0)
            fclose(g_FILE_cv_0);

        // destroy critical sections
        DeleteCriticalSection(&g_csRead);
        DeleteCriticalSection(&_cs_savedTki);
        DeleteCriticalSection(&_cs_slots);
	}
	
	return true;
}

void InitSlotMap()
{
    if (!g_teamKitInfoBase)
    {
        g_teamKitInfoBase = *(DWORD*)data[TEAM_KIT_INFO_BASE] + 0xe8bc4;
        if (!g_teamKitInfoBase)
        {
            LOG(L"ERROR: Unable to initialize slot maps: g_teamKitInfoBase = 0");
            return;
        }
    }

    TEAM_KIT_INFO* teamKitInfo = (TEAM_KIT_INFO*)g_teamKitInfoBase;
    for (WORD i=0; i<NUM_REGULAR_TEAMS; i++)
    {
        if (teamKitInfo[i].pa.kitLink < NUM_SLOTS) // teams with kit slots in AFS 
        {
            g_kitSlotByTeamId.insert(pair<WORD,WORD>(i,teamKitInfo[i].pa.kitLink));
            g_teamIdByKitSlot.insert(pair<WORD,WORD>(teamKitInfo[i].pa.kitLink,i));
        }
    }
    LOG(L"Slot maps initialized.");
}

void kservReadTeamKitInfoCallPoint1()
{
    // this is helper function so that we can
    // hook kservAfterReadTeamKitInfo() at a certain place: right after the
    // TEAM_KIT_INFO structure was read.
    __asm 
    {
        pushfd
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        sub esi,0x200  // go back to original offset - before copy
        sub edi,0x200  // go back to original offset - before copy
        push esi
        push edi
        call kservAfterReadTeamKitInfo
        add esp,8     // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        mov dword ptr ss:[esp+0x2f4],0  // execute replaced code (stack offset: extra 4 bytes)
        retn
    }
}

void kservReadTeamKitInfoCallPoint2()
{
    // this is helper function so that we can
    // hook kservAfterReadTeamKitInfo() at a certain place: right after the
    // TEAM_KIT_INFO structure was read.
    __asm 
    {
        pushfd
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        sub esi,0x200  // go back to original offset - before copy
        sub edi,0x200  // go back to original offset - before copy
        push esi
        push edi
        call kservAfterReadTeamKitInfo
        add esp,8     // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        mov esi,dword ptr ss:[esp+0x2c0]  // execute replaced code (stack offset: extra 4 bytes)
        retn
    }
}

void kservReadTeamKitInfoCallPoint3()
{
    // this is helper function so that we can
    // hook kservAfterReadTeamKitInfo() at a certain place: right after the
    // TEAM_KIT_INFO structure was read.
    __asm 
    {
        pushfd
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        rep movs dword ptr es:[edi], dword ptr ds:[esi] // execute replaced code
        sub esi,0x200  // go back to original offset - before copy
        sub edi,0x200  // go back to original offset - before copy
        push esi
        push edi
        call kservAfterReadTeamKitInfo
        add esp,8     // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        xor esi,esi // execute replaced code
        cmp ebx,esi
        retn
    }
}


void kservStartReadKitInfoCallPoint()
{
    __asm 
    {
        pushfd
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push esi  // parameter
        call kservStartReadKitInfo
        add esp,4     // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        add esi,0x88  // execute replaced code
        retn
    }
}

void kservStartReadKitInfoCallPoint2()
{
    __asm 
    {
        pushfd
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push edi  // parameter
        call kservStartReadKitInfo
        add esp,4     // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        mov edx,dword ptr ds:[eax+edi+8]  // execute replaced code
        lea eax,dword ptr ds:[eax+edi+8]
        retn
    }
}

void kservStartReadKitInfoCallPoint3()
{
    __asm 
    {
        pushfd
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push ecx  // parameter
        call kservStartReadKitInfo
        add esp,4     // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        lea eax,dword ptr ds:[eax+ecx+0x88] // execute repaced code
        retn
    }
}

void kservEndReadKitInfoCallPoint()
{
    __asm 
    {
        pushfd
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        call kservEndReadKitInfo
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        add esp, 4 // execute replaced code (adjust stack a bit)
        retn 4 
    }
}

void kservOnEnterCupsCallPoint()
{
    __asm 
    {
        pushfd
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        mov edi,g_cupModeInd // execute repaced code
        mov [edi],esi
        call kservOnEnterCups
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        retn
    }
}

void kservOnLeaveCupsCallPoint()
{
    __asm 
    {
        pushfd
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        mov esi,g_cupModeInd // execute repaced code
        mov [esi],ebp
        call kservOnLeaveCups
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        retn
    }
}

void kservOnLeaveCupsCallPoint2()
{
    __asm 
    {
        pushfd
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        call kservOnLeaveCups
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        add esp,4  // execute replaced code
        retn
    }
}

void setQualityChecks()
{
	DWORD protection = 0;
	DWORD newProtection = PAGE_EXECUTE_READWRITE;
	BYTE* bptr = NULL;
	
	if (code[C_CONTROLLERFIX1] != 0 && code[C_CONTROLLERFIX2] != 0)	{
		for (int i=0; i<2; i++) {
			bptr = (BYTE*)code[C_CONTROLLERFIX1 + i];
	    if (VirtualProtect(bptr, 7, newProtection, &protection)) {
	    	memset(bptr, 0x90, 7);
			}
		}
	}
	
	if (allQualities && code[C_QUALITYCHECK1] != 0 && code[C_QUALITYCHECK2] != 0)	{
		for (int i=0; i<2; i++) {
			bptr = (BYTE*)code[C_QUALITYCHECK1 + i];
	    if (VirtualProtect(bptr, 2, newProtection, &protection)) {
	    	/* NOP */ bptr[0] = 0x90;
	    	/* Jmp... */ bptr[1] = 0xe9;
            TRACE1N(L"Quality check #%d disabled",i);
			}
		}
	}
}

void kservConfig(char* pName, const void* pValue, DWORD a)
{
	switch (a) {
		case 1: // debug
			k_kserv.debug = *(DWORD*)pValue;
			break;
        case 2: // use-description
            _kserv_config._use_description = *(DWORD*)pValue == 1;
            break;
	}
	return;
}

HRESULT STDMETHODCALLTYPE initKserv(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface) {
	TRACE(L"Going to hook some functions now.");
	
	#ifndef MYDLL_RELEASE_BUILD
	hookFunction(hk_RenderPlayer, kservRenderPlayer);
	#endif
	
	TRACE(L"Hooking done.");
	unhookFunction(hk_D3D_CreateDevice, initKserv);
	unhookFunction(hk_D3D_Create, setQualityChecks);

    // Load GDB
    LOG1S(L"pesDir: {%s}",getPesInfo()->pesDir);
    LOG1S(L"myDir : {%s}",getPesInfo()->myDir);
    LOG1S(L"gdbDir: {%sGDB}",getPesInfo()->gdbDir);
    gdb = new GDB(getPesInfo()->gdbDir, false);
    LOG1N(L"Teams in GDB map: %d", gdb->uni.size());

    getConfig("kserv", "debug", DT_DWORD, 1, kservConfig);
    getConfig("kserv", "use.description", DT_DWORD, 2, kservConfig);

    LOG1N(L"debug = %d", k_kserv.debug);
    LOG1N(L"use.description = %d", _kserv_config._use_description);

    // Initial iterators reset
    ResetIterators();

    // hook UnpackBin
    //MasterHookFunction(code[C_UNPACK_BIN], 2, kservUnpackBin);
    _orgUnpackBin = (PROC_UNPACK_BIN)GetTargetAddress(code[C_UNPACK_BIN]);
    HookCallPoint(code[C_UNPACK_BIN], kservUnpackBinCallPoint, 6, 0);
    
    // hook reading of team kit info
    HookCallPoint(code[C_AFTER_READTEAMKITINFO_1], kservReadTeamKitInfoCallPoint1, 6, data[NUMNOPS_1]);
    HookCallPoint(code[C_AFTER_READTEAMKITINFO_2], kservReadTeamKitInfoCallPoint2, 6, data[NUMNOPS_2]);
    HookCallPoint(code[C_AFTER_READTEAMKITINFO_3], kservReadTeamKitInfoCallPoint3, 6, 1);
    HookCallPoint(code[C_START_READKITINFO], kservStartReadKitInfoCallPoint, 6, 1);
    HookCallPoint(code[C_START_READKITINFO_2], kservStartReadKitInfoCallPoint2, 6, 3);
    HookCallPoint(code[C_START_READKITINFO_3], kservStartReadKitInfoCallPoint3, 6, 2);
    HookCallPoint(code[C_END_READKITINFO], kservEndReadKitInfoCallPoint, 6, 0);

    // add callbacks
    addKeyboardCallback(kservKeyboardEvent);
    addReadReplayDataCallback(kservReadReplayData);
    addWriteReplayDataCallback(kservWriteReplayData);
    addMenuCallback(kservMenuEvent);

    // hook mode enter/exit points
    g_cupModeInd = data[CUP_MODE_PTR];
    HookCallPoint(code[C_ON_ENTER_CUPS], kservOnEnterCupsCallPoint, 6, 1);
    HookCallPoint(code[C_ON_LEAVE_CUPS], kservOnLeaveCupsCallPoint, 6, 1);
    HookCallPoint(code[C_ON_LEAVE_CUPS_2], kservOnLeaveCupsCallPoint2, 6, 0);

    HookCallPoint(code[C_AT_READ_KIT_CHOICE],
            kservAtReadKitSelectionCallPoint, 6, 1);
    HookCallPoint(code[C_AT_READ_KIT_CHOICE_2],
            kservAtReadKitSelectionCallPoint2, 6, 1);

    // hook team selection point
    //HookCallPoint(code[C_AT_WRITE_TEAM_ID], 
    //        kservAtWriteTeamIdCallPoint, 6, 0);

    HookCallPoint(code[C_PTR_CHECK], kservPtrCheckCallPoint, 6, 0);

    // hook bin loading interceptor
    switch (getPesInfo()->gameVersion)
    {
        case gvPES2008v120:
            HookCallPoint(code[C_BEFORE_LOAD_BIN], kservBeforeLoadBinCallPoint_120, 6, 2);
            break;
        default:
            HookCallPoint(code[C_BEFORE_LOAD_BIN], kservBeforeLoadBinCallPoint, 6, 2);
            break;
    }
    
    return D3D_OK;
}

void HookAt(DWORD addr, void* func)
{
    DWORD target = (DWORD)func;
	if (addr && target)
	{
	    BYTE* bptr = (BYTE*)addr;
	    DWORD protection = 0;
	    DWORD newProtection = PAGE_EXECUTE_READWRITE;
	    if (VirtualProtect(bptr, 8, newProtection, &protection)) {
	        bptr[0] = 0xe8;
	        DWORD* ptr = (DWORD*)(addr + 1);
	        ptr[0] = target - (DWORD)(addr + 5);
	        LOG2N(L"Function (%08x) HOOKED at address (%08x)", target, addr);
	    }
	}
}

void DumpTexture(IDirect3DTexture9* const ptexture) 
{
    wchar_t buf[BUFLEN];
    swprintf(buf, L"kitserver\\tex-%08x.dds", (DWORD)ptexture);
    D3DXSaveTextureToFile(buf, D3DXIFF_DDS, ptexture, NULL);
}

IDirect3DTexture9* bigTex = NULL;
IDirect3DTexture9* smallTex = NULL;
void kservRenderPlayer(TexPlayerInfo* tpi, DWORD coll, DWORD num, WORD* orgTexIds, BYTE orgTexMaxNum)
{
	if (tpi->referee) return;
	if (!tpi->isGk) return;
	
	if (((tpi->lod == 0) && (num != 0)) ||
		  ((tpi->lod == 1) && (num != 1)) ||
		  ((tpi->lod > 1) && (num != 2)))
		return;
		
	if (!bigTex) {
		D3DXCreateTextureFromFile(getActiveDevice(), L"E:\\Pro Evolution Soccer 2008 DEMO\\kitserver\\GDB\\chelsea.png", &bigTex);
		smallTex = bigTex;
		//DumpTexture(bigTex);
	}

	for (int i = 0; i < orgTexMaxNum; i++) {
		if (orgTexIds[i] == 0x69) {
			setNewSubTexture(coll, i, bigTex);
		} else if (orgTexIds[i] == 0x70) {
			setNewSubTexture(coll, i, smallTex);
		}
	}
	return;
}

bool IsKitBin(DWORD afsId, DWORD binId)
{
    if (afsId == 0)
    {
        if (FIRST_KIT_BIN <= binId && binId < FIRST_KIT_BIN + NUM_SLOTS*2) 
            return true;
        else if (FIRST_NUMS_BIN <= binId && binId < FIRST_NUMS_BIN + NUM_SLOTS*4) 
            return true;
        else if (FIRST_FONT_BIN <= binId && binId < FIRST_FONT_BIN + NUM_SLOTS*4) 
            return true;
    }
    else if (afsId == PDATA_IMG)
    {
        if (binId == SOUTH_KOREA_GK || binId == SOUTH_KOREA_PL)
            return true;
    }
    return false;
}

void kservBeforeLoadBinCallPoint()
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
        mov ecx,esi
        sub ecx,4
        mov edx,[esi+0x28]
        push ecx
        push edx
        call kservBeforeLoadBin
        add esp,8     // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        mov ecx,dword ptr ds:[esi+0x28] // execute replaced code
        lea eax,dword ptr ss:[esp+0x18]
        retn
    }
}

void kservBeforeLoadBinCallPoint_120()
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
        mov ecx,esi
        mov edx,[esi+0x2c]
        push ecx
        push edx
        call kservBeforeLoadBin
        add esp,8     // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        mov ecx,dword ptr ds:[esi+0x2c] // execute replaced code
        lea eax,dword ptr ss:[esp+0x18]
        retn
    }
}

KEXPORT void kservBeforeLoadBin(LOAD_BIN_STRUCT* s1, LOAD_BIN_STRUCT* s2)
{
    TRACE2N(L"Loading BIN #%d, afsId=%d",s1->binId,s1->afsId);
    if (s1->afsId!=s2->afsId || s1->binId!=s2->binId)
    {
        TRACE4N(L"Structures differ: (%d,%d) vs. (%d,%d)",
                s1->afsId, s1->binId,
                s2->afsId, s2->binId);

        // special check for South Korea
        if (!(s1->afsId==PDATA_IMG && s1->binId==SOUTH_KOREA_GK 
                    && s2->afsId==0 && s2->binId==SOUTH_KOREA_GK_CV0)
            && !(s1->afsId==PDATA_IMG && s1->binId==SOUTH_KOREA_PL 
                && s2->afsId==0 && s2->binId==SOUTH_KOREA_PL_CV0))
        return;
    }

    if (IsKitBin(s1->afsId,s1->binId))
    {
        if (k_kserv.debug)
            LOG2N(L"Kit bin: (%d,%d)",s1->afsId,s1->binId);
        EnterCriticalSection(&g_csRead);
        DWORD threadId = GetCurrentThreadId();
        hash_map<DWORD,READ_BIN_STRUCT>::iterator it = g_read_bins.find(threadId);
        if (it == g_read_bins.end())
        {
            // insert new entry
            READ_BIN_STRUCT rbs;
            rbs.afsId = s1->afsId;
            rbs.binId = s1->binId;
            g_read_bins.insert(pair<DWORD,READ_BIN_STRUCT>(threadId,rbs));
        }
        else
        {
            // update entry (shouldn't happen, really)
            it->second.afsId = s1->afsId; 
            it->second.binId = s1->binId; 
            TRACE2N(L"WARNING: updated entry for #%d, afsId=%d",s1->binId,s1->afsId);
        }
        LeaveCriticalSection(&g_csRead);

        DWORD binId = s1->binId;

        // determine type of bin
        int type = GetBinType(binId);

        // determine team ID by kit slot
        WORD kitSlot = GetKitSlot(binId);
        EnterCriticalSection(&_cs_slots);
        hash_map<WORD,WORD>::iterator ksit = g_teamIdByKitSlot.find(kitSlot);
        if (ksit != g_teamIdByKitSlot.end())
        {
            WORD teamId = ksit->second;

            // set iterators
            map<wstring,Kit>::iterator iterPL = g_iterHomePL_end;
            map<wstring,Kit>::iterator iterGK = g_iterHomeGK_end;
            map<wstring,Kit>::iterator iterPL_end = g_iterHomePL_end;
            map<wstring,Kit>::iterator iterGK_end = g_iterHomeGK_end;
            NEXT_MATCH_DATA_INFO* pNM = *(NEXT_MATCH_DATA_INFO**)data[NEXT_MATCH_DATA_PTR];
            if (pNM && pNM->home && pNM->away)
            {
                iterPL = (pNM->home->teamId == teamId) ? g_iterHomePL : g_iterAwayPL;
                iterGK = (pNM->home->teamId == teamId) ? g_iterHomeGK : g_iterAwayGK;
                iterPL_end = (pNM->home->teamId == teamId) ? g_iterHomePL_end : g_iterAwayPL_end;
                iterGK_end = (pNM->home->teamId == teamId) ? g_iterHomeGK_end : g_iterAwayGK_end;
            }

            int shiftGK[] = {0,0};
            if (iterGK != iterGK_end)
            { 
                shiftGK[0] = (iterGK->first == L"ga") ? 0 : 1;
                shiftGK[1] = (iterGK->first == L"ga") ? -1 : 0;
            }
            int shiftPL[] = {0,0};
            if (iterPL != iterPL_end)
            { 
                shiftPL[0] = (iterPL->first == L"pa") ? 0 : 1;
                shiftPL[1] = (iterPL->first == L"pa") ? -1 : 0;
            }

            switch (type)
            {
                case BIN_FONT_GA:
                case BIN_NUMS_GA:
                    if (shiftGK[0] != 0)
                    {
                        s1->binId = binId + shiftGK[0];
                        s2->binId = binId + shiftGK[0];
                    }
                    break;
                case BIN_FONT_GB:
                case BIN_NUMS_GB:
                    if (shiftGK[1] != 0)
                    {
                        s1->binId = binId + shiftGK[1];
                        s2->binId = binId + shiftGK[1];
                    }
                    break;
                case BIN_FONT_PA:
                case BIN_NUMS_PA:
                    if (shiftPL[0] != 0)
                    {
                        s1->binId = binId + shiftPL[0];
                        s2->binId = binId + shiftPL[0];
                    }
                    break;
                case BIN_FONT_PB:
                case BIN_NUMS_PB:
                    if (shiftPL[1] != 0)
                    {
                        s1->binId = binId + shiftPL[1];
                        s2->binId = binId + shiftPL[1];
                    }
                    break;
            }
        }
        LeaveCriticalSection(&_cs_slots);
    } // end IsKitBin
    TRACE2N(L"Loading BIN done for #%d, afsId=%d",s1->binId,s1->afsId);
}

void kservUnpackBinCallPoint()
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
        mov esi,[esp+0x24]
        mov edi,[esp+0x20]
        push esi
        push edi
        call kservUnpackBin
        add esp,8     // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop ebp
        popfd
        retn
    }
}

KEXPORT DWORD kservUnpackBin(UNPACK_INFO* pUnpackInfo, DWORD p2)
{
    DWORD afsId = 0;
    DWORD currBin = 0xffffffff;
    EnterCriticalSection(&g_csRead);
    hash_map<DWORD,READ_BIN_STRUCT>::iterator bit = g_read_bins.find(GetCurrentThreadId());
    if (bit != g_read_bins.end())
    {
        currBin = bit->second.binId;
        afsId = bit->second.afsId;
        g_read_bins.erase(bit);
    }
    LeaveCriticalSection(&g_csRead);

    // save the pointer, because the unpacking call will change it.
    UNPACKED_BIN* bin = (UNPACKED_BIN*)pUnpackInfo->destBuffer;

    // call original
    DWORD result = _orgUnpackBin(pUnpackInfo, p2);
    //DWORD result = MasterCallNext(pUnpackInfo, p2);

    if (IsKitBin(afsId,currBin))
    {
        TRACE2N(L"kservUnpackBin:: afsId=%d, binId=%d",afsId,currBin);
        TRACE1N(L"kservUnpackBin:: num entries = %d", bin->header.numEntries);
        //DumpData((BYTE*)bin, pUnpackInfo->destSize);

        // determine type of bin
        int type = GetBinType(currBin);

        // determine team ID by kit slot
        WORD kitSlot = GetKitSlot(currBin);
        EnterCriticalSection(&_cs_slots);
        hash_map<WORD,WORD>::iterator ksit = g_teamIdByKitSlot.find(kitSlot);
        if (ksit != g_teamIdByKitSlot.end())
        {
            LeaveCriticalSection(&_cs_slots);
            WORD teamId = ksit->second;

            // First, operate with bins from AFS
            {
                // set iterators
                map<wstring,Kit>::iterator iterPL;
                map<wstring,Kit>::iterator iterGK;
                map<wstring,Kit>::iterator iterPL_end;
                map<wstring,Kit>::iterator iterGK_end;
                NEXT_MATCH_DATA_INFO* pNM = *(NEXT_MATCH_DATA_INFO**)data[NEXT_MATCH_DATA_PTR];
                if (pNM && pNM->home && pNM->away)
                {
                    TRACE2N(L"kservUnpackBin (AFS): teams: %d vs %d", pNM->home->teamId, pNM->away->teamId);
                    iterPL = (pNM->home->teamId == teamId) ? g_iterHomePL : g_iterAwayPL;
                    iterGK = (pNM->home->teamId == teamId) ? g_iterHomeGK : g_iterAwayGK;
                    iterPL_end = (pNM->home->teamId == teamId) ? g_iterHomePL_end : g_iterAwayPL_end;
                    iterGK_end = (pNM->home->teamId == teamId) ? g_iterHomeGK_end : g_iterAwayGK_end;
                } 

                wstring keysGK[] = {L"",L""};
                if (iterGK == iterGK_end) { keysGK[0] = L"ga"; keysGK[1] = L"gb"; }
                else { keysGK[0] = iterGK->first; keysGK[1] = iterGK->first; }
                wstring keysPL[] = {L"",L""};
                if (iterPL == iterPL_end) { keysPL[0] = L"pa"; keysPL[1] = L"pb"; }
                else { keysPL[0] = iterPL->first; keysPL[1] = iterPL->first; }

                // copy textures in place
                if (bin->header.numEntries == 2)/* && 
                        bin->entryInfo[0].size == 0x40410 &&
                        bin->entryInfo[1].size == 0x40410)*/
                {
                    switch (type)
                    {
                        case BIN_KIT_GK:
                            {
                                if (keysGK[1] == L"ga") {
                                    // make 2 home textures
                                    TEXTURE_ENTRY* tex1 = (TEXTURE_ENTRY*)((BYTE*)bin + bin->entryInfo[0].offset);
                                    TEXTURE_ENTRY* tex2 = (TEXTURE_ENTRY*)((BYTE*)bin + bin->entryInfo[1].offset);
                                    memcpy(tex2, tex1, bin->entryInfo[0].size);
                                }
                                else if (keysGK[0] == L"gb") {
                                    // make 2 away textures
                                    TEXTURE_ENTRY* tex1 = (TEXTURE_ENTRY*)((BYTE*)bin + bin->entryInfo[0].offset);
                                    TEXTURE_ENTRY* tex2 = (TEXTURE_ENTRY*)((BYTE*)bin + bin->entryInfo[1].offset);
                                    memcpy(tex1, tex2, bin->entryInfo[0].size);
                                }
                            }
                            break;
                        case BIN_KIT_PL:
                            {
                                if (keysPL[1] == L"pa") {
                                    // make 2 home textures
                                    TEXTURE_ENTRY* tex1 = (TEXTURE_ENTRY*)((BYTE*)bin + bin->entryInfo[0].offset);
                                    TEXTURE_ENTRY* tex2 = (TEXTURE_ENTRY*)((BYTE*)bin + bin->entryInfo[1].offset);
                                    memcpy(tex2, tex1, bin->entryInfo[0].size);
                                }
                                else if (keysPL[0] == L"pb") {
                                    // make 2 away textures
                                    TEXTURE_ENTRY* tex1 = (TEXTURE_ENTRY*)((BYTE*)bin + bin->entryInfo[0].offset);
                                    TEXTURE_ENTRY* tex2 = (TEXTURE_ENTRY*)((BYTE*)bin + bin->entryInfo[1].offset);
                                    memcpy(tex1, tex2, bin->entryInfo[0].size);
                                }
                            }
                            break;
                    }
                }
                else if (bin->header.numEntries == 4 &&
                        bin->entryInfo[2].size == 0x8410 &&
                        bin->entryInfo[3].size == 0x8410)
                {
                    // need to correct the short-numbers textures in NUMS bins
                    switch (type)
                    {
                        case BIN_NUMS_PA:
                        case BIN_NUMS_PB:
                            {
                                if (keysPL[1] == L"pa") {
                                    // make 2 home textures
                                    TEXTURE_ENTRY* tex1 = (TEXTURE_ENTRY*)((BYTE*)bin + bin->entryInfo[2].offset);
                                    TEXTURE_ENTRY* tex2 = (TEXTURE_ENTRY*)((BYTE*)bin + bin->entryInfo[3].offset);
                                    memcpy(tex2, tex1, bin->entryInfo[2].size);
                                }
                                else if (keysPL[0] == L"pb") {
                                    // make 2 away textures
                                    TEXTURE_ENTRY* tex1 = (TEXTURE_ENTRY*)((BYTE*)bin + bin->entryInfo[2].offset);
                                    TEXTURE_ENTRY* tex2 = (TEXTURE_ENTRY*)((BYTE*)bin + bin->entryInfo[3].offset);
                                    memcpy(tex1, tex2, bin->entryInfo[2].size);
                                }
                            }
                            break;
                        case BIN_NUMS_GA:
                        case BIN_NUMS_GB:
                            {
                                if (keysGK[1] == L"ga") {
                                    // make 2 home textures
                                    TEXTURE_ENTRY* tex1 = (TEXTURE_ENTRY*)((BYTE*)bin + bin->entryInfo[2].offset);
                                    TEXTURE_ENTRY* tex2 = (TEXTURE_ENTRY*)((BYTE*)bin + bin->entryInfo[3].offset);
                                    memcpy(tex2, tex1, bin->entryInfo[2].size);
                                }
                                else if (keysGK[0] == L"gb") {
                                    // make 2 away textures
                                    TEXTURE_ENTRY* tex1 = (TEXTURE_ENTRY*)((BYTE*)bin + bin->entryInfo[2].offset);
                                    TEXTURE_ENTRY* tex2 = (TEXTURE_ENTRY*)((BYTE*)bin + bin->entryInfo[3].offset);
                                    memcpy(tex1, tex2, bin->entryInfo[2].size);
                                }
                            }
                            break;
                    }
                }
            }

            // Second: replace textures from GDB, if found
            hash_map<WORD,KitCollection>::iterator wkit = gdb->uni.find(teamId);
            if (wkit != gdb->uni.end())
            {
                // set iterators
                map<wstring,Kit>::iterator iterPL;
                map<wstring,Kit>::iterator iterGK;
                map<wstring,Kit>::iterator iterPL_end;
                map<wstring,Kit>::iterator iterGK_end;
                NEXT_MATCH_DATA_INFO* pNM = *(NEXT_MATCH_DATA_INFO**)data[NEXT_MATCH_DATA_PTR];
                if (pNM && pNM->home && pNM->away)
                {
                    TRACE2N(L"kservUnpackBin: teams: %d vs %d", pNM->home->teamId, pNM->away->teamId);
                    iterPL = (pNM->home->teamId == teamId) ? g_iterHomePL : g_iterAwayPL;
                    iterGK = (pNM->home->teamId == teamId) ? g_iterHomeGK : g_iterAwayGK;
                    iterPL_end = (pNM->home->teamId == teamId) ? g_iterHomePL_end : g_iterAwayPL_end;
                    iterGK_end = (pNM->home->teamId == teamId) ? g_iterHomeGK_end : g_iterAwayGK_end;
                } 

                wstring keysGK[] = {L"",L""};
                if (iterGK == iterGK_end) { keysGK[0] = L"ga"; keysGK[1] = L"gb"; }
                else { keysGK[0] = iterGK->first; keysGK[1] = iterGK->first; }
                wstring keysPL[] = {L"",L""};
                if (iterPL == iterPL_end) { keysPL[0] = L"pa"; keysPL[1] = L"pb"; }
                else { keysPL[0] = iterPL->first; keysPL[1] = iterPL->first; }

                // found this team in GDB.
                // replace textures
                if (bin->header.numEntries == 2)/* && 
                        bin->entryInfo[0].size == 0x40410 &&
                        bin->entryInfo[1].size == 0x40410)*/
                {
                    wchar_t filename[BUFLEN] = {L'\0'};
                    switch (type)
                    {
                        case BIN_KIT_GK:
                            {
                                wstring files[] = {
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\" + keysGK[0] + L"\\kit.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\" + keysGK[1] + L"\\kit.png",
                                };
                                ReplaceTexturesInBin(bin, files, 2);
                            }
                            break;
                        case BIN_KIT_PL:
                            {
                                wstring files[] = {
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\" + keysPL[0] + L"\\kit.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\" + keysPL[1] + L"\\kit.png",
                                };
                                ReplaceTexturesInBin(bin, files, 2);
                            }
                            break;
                    }
                }

                else if (bin->header.numEntries == 1 || bin->header.numEntries == 4)
                {
                    switch (type)
                    {
                        case BIN_FONT_GA:
                            {
                                wstring files[] = {
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\" + keysGK[0] + L"\\font.png",
                                };
                                ReplaceTexturesInBin(bin, files, 1);
                            }
                            break;
                        case BIN_FONT_GB:
                            {
                                wstring files[] = {
                                        gdb->dir + L"GDB\\uni\\"
                                            + wkit->second.foldername 
                                            + L"\\" + keysGK[1] + L"\\font.png",
                                };
                                ReplaceTexturesInBin(bin, files, 1);
                            }
                            break;
                        case BIN_FONT_PA:
                            {
                                wstring files[] = {
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\" + keysPL[0] + L"\\font.png",
                                };
                                ReplaceTexturesInBin(bin, files, 1);
                            }
                            break;
                        case BIN_FONT_PB:
                            {
                                wstring files[] = {
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\" + keysPL[1] + L"\\font.png",
                                };
                                ReplaceTexturesInBin(bin, files, 1);
                            }
                            break;
                        case BIN_NUMS_GA:
                            {
                                wstring files[] = {
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\" + keysGK[0] + L"\\numbers-back.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\" + keysGK[0] + L"\\numbers-front.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\" + keysGK[0] + L"\\numbers-shorts.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\" + keysGK[0] + L"\\numbers-shorts.png",
                                };
                                ReplaceTexturesInBin(bin, files, 4);
                            }
                            break;
                        case BIN_NUMS_GB:
                            {
                                wstring files[] = {
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\" + keysGK[1] + L"\\numbers-back.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\" + keysGK[1] + L"\\numbers-front.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\" + keysGK[1] + L"\\numbers-shorts.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\" + keysGK[1] + L"\\numbers-shorts.png",
                                };
                                ReplaceTexturesInBin(bin, files, 4);
                            }
                            break;
                        case BIN_NUMS_PA:
                            {
                                wstring files[] = {
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\" + keysPL[0] + L"\\numbers-back.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\" + keysPL[0] + L"\\numbers-front.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\" + keysPL[0] + L"\\numbers-shorts.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\" + keysPL[0] + L"\\numbers-shorts.png",
                                };
                                ReplaceTexturesInBin(bin, files, 4);
                            }
                            break;
                        case BIN_NUMS_PB:
                            {
                                wstring files[] = {
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\" + keysPL[1] + L"\\numbers-back.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\" + keysPL[1] + L"\\numbers-front.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\" + keysPL[1] + L"\\numbers-shorts.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\" + keysPL[1] + L"\\numbers-shorts.png",
                                };
                                ReplaceTexturesInBin(bin, files, 4);
                            }
                            break;
                    }
                }
            } // end if wkit

        } // end if ksit
        else
        {
            LeaveCriticalSection(&_cs_slots);
        }
    }

    return result;
}

void ReplaceTexturesInBin(UNPACKED_BIN* bin, wstring files[], size_t n)
{
    for (int i=0; i<n; i++)
    {
        TEXTURE_ENTRY* tex = (TEXTURE_ENTRY*)((BYTE*)bin + bin->entryInfo[i].offset);
        BITMAPINFO* bmp = NULL;
        DWORD texSize = LoadPNGTexture(&bmp, files[i].c_str());
        if (texSize)
        {
            ApplyDIBTexture(tex, bmp);
            FreePNGTexture(bmp);
        }
    }
}

void DumpData(void* data, size_t size)
{
    static int count = 0;
    char filename[256] = {0};
    sprintf(filename, "kitserver/dump%03d.bin", count);
    FILE* f = fopen(filename,"wb");
    if (f) 
    {
        fwrite(data, size, 1, f);
        fclose(f);
        LOG1N(L"Dumped file (count=%d)",count);
    }
    else
    {
        LOG2N(L"ERROR: unable to dump file (count=%d). Error code = %d",count, errno);
    }
    count++;
}

// Load texture from PNG file. Returns the size of loaded texture
DWORD LoadPNGTexture(BITMAPINFO** tex, const wchar_t* filename)
{
    if (k_kserv.debug)
        LOG1S(L"LoadPNGTexture: loading %s", (wchar_t*)filename);
    DWORD size = 0;

    PNGDIB *pngdib;
    LPBITMAPINFOHEADER* ppDIB = (LPBITMAPINFOHEADER*)tex;

    pngdib = pngdib_p2d_init();
	//TRACE(L"LoadPNGTexture: structure initialized");

    BYTE* memblk;
    int memblksize;
    if(read_file_to_mem(filename,&memblk, &memblksize)) {
        LOG(L"LoadPNGTexture: unable to read PNG file");
        return 0;
    }
    //TRACE(L"LoadPNGTexture: file read into memory");

    pngdib_p2d_set_png_memblk(pngdib,memblk,memblksize);
	pngdib_p2d_set_use_file_bg(pngdib,1);
	pngdib_p2d_run(pngdib);

	//TRACE(L"LoadPNGTexture: run done");
    pngdib_p2d_get_dib(pngdib, ppDIB, (int*)&size);
	//TRACE(L"LoadPNGTexture: get_dib done");

    pngdib_done(pngdib);
	TRACE(L"LoadPNGTexture: done done");

	TRACE1N(L"LoadPNGTexture: *ppDIB = %08x", (DWORD)*ppDIB);
    if (*ppDIB == NULL) {
        LOG(L"LoadPNGTexture: ERROR - unable to load PNG image.");
        return 0;
    }

    // read transparency values from tRNS chunk
    // and put them into DIB's RGBQUAD.rgbReserved fields
    ApplyAlphaChunk((RGBQUAD*)&((BITMAPINFO*)*ppDIB)->bmiColors, memblk, memblksize);

    HeapFree(GetProcessHeap(), 0, memblk);

	TRACE(L"LoadPNGTexture: done");
	return size;
}

/**
 * Extracts alpha values from tRNS chunk and applies stores
 * them in the RGBQUADs of the DIB
 */
void ApplyAlphaChunk(RGBQUAD* palette, BYTE* memblk, DWORD size)
{
    bool got_alpha = false;

    // find the tRNS chunk
    DWORD offset = 8;
    while (offset < size) {
        PNG_CHUNK_HEADER* chunk = (PNG_CHUNK_HEADER*)(memblk + offset);
        if (chunk->dwName == MAKEFOURCC('t','R','N','S')) {
            int numColors = SWAPBYTES(chunk->dwSize);
            BYTE* alphaValues = memblk + offset + sizeof(chunk->dwSize) + sizeof(chunk->dwName);
            for (int i=0; i<numColors; i++) {
                palette[i].rgbReserved = alphaValues[i];
            }
            got_alpha = true;
            break;
        }
        // move on to next chunk
        offset += sizeof(chunk->dwSize) + sizeof(chunk->dwName) + 
            SWAPBYTES(chunk->dwSize) + sizeof(DWORD); // last one is CRC
    }

    // initialize alpha to all-opaque, if haven't gotten it
    if (!got_alpha) {
        LOG(L"ApplyAlphaChunk::WARNING: no transparency.");
        for (int i=0; i<256; i++) {
            palette[i].rgbReserved = 0xff;
        }
    }
}

// Read a file into a memory block.
static int read_file_to_mem(const wchar_t *fn,unsigned char **ppfiledata, int *pfilesize)
{
	HANDLE hfile;
	DWORD fsize;
	//unsigned char *fbuf;
	BYTE* fbuf;
	DWORD bytesread;

	hfile=CreateFile(fn,GENERIC_READ,FILE_SHARE_READ,NULL,
		OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if(hfile==INVALID_HANDLE_VALUE) return 1;

	fsize=GetFileSize(hfile,NULL);
	if(fsize>0) {
		//fbuf=(unsigned char*)GlobalAlloc(GPTR,fsize);
		//fbuf=(unsigned char*)calloc(fsize,1);
        fbuf = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, fsize);
		if(fbuf) {
			if(ReadFile(hfile,(void*)fbuf,fsize,&bytesread,NULL)) {
				if(bytesread==fsize) { 
					(*ppfiledata)  = fbuf;
					(*pfilesize) = (int)fsize;
					CloseHandle(hfile);
					return 0;   // success
				}
			}
			free((void*)fbuf);
		}
	}
	CloseHandle(hfile);
	return 1;  // error
}

// Substitute kit textures with data from DIB
// Currently supports only 4bit and 8bit paletted DIBs
void ApplyDIBTexture(TEXTURE_ENTRY* tex, BITMAPINFO* bitmap)
{
    TRACE(L"Applying DIB texture");

	BYTE* srcTex = (BYTE*)bitmap;
	BITMAPINFOHEADER* bih = &bitmap->bmiHeader;
	DWORD palOff = bih->biSize;
    DWORD numColors = bih->biClrUsed;
    if (numColors == 0)
        numColors = 1 << bih->biBitCount;

    DWORD palSize = numColors*4;
	DWORD bitsOff = palOff + palSize;

    if (bih->biBitCount!=4 && bih->biBitCount!=8)
    {
        LOG(L"ERROR: Unsupported bit-depth. Must be 4- or 8-bit paletted image.");
        return;
    }
    TRACE1N(L"Loading %d-bit image...", bih->biBitCount);

	// copy palette
	TRACE1N(L"bitsOff = %08x", bitsOff);
	TRACE1N(L"palOff  = %08x", palOff);
    memset((BYTE*)&tex->palette, 0, 0x400);
    memcpy((BYTE*)&tex->palette, srcTex + palOff, palSize);
	// swap R and B
	for (int i=0; i<numColors; i++) 
	{
		BYTE blue = tex->palette[i].b;
		BYTE red = tex->palette[i].r;
		tex->palette[i].b = red;
		tex->palette[i].r = blue;
	}
	TRACE(L"Palette copied.");

	int k, m, j, w;
    int width = min(tex->header.width, bih->biWidth); // be safe
    int height = min(tex->header.height, bih->biHeight); // be safe

	// copy pixel data
    if (bih->biBitCount == 8)
    {
        for (k=0, m=bih->biHeight-1; k<height, m>=bih->biHeight - height; k++, m--)
        {
            memcpy(tex->data + k*tex->header.width, srcTex + bitsOff + m*width, width);
        }
    }
    else if (bih->biBitCount == 4)
    {
        for (k=0, m=bih->biHeight-1; k<tex->header.height, m>=0; k++, m--)
        {
            for (j=0; j<bih->biWidth/2; j+=1) {
                // expand ech nibble into full byte
                tex->data[k*(tex->header.width) + j*2] = srcTex[bitsOff + m*(bih->biWidth/2) + j] >> 4 & 0x0f;
                tex->data[k*(tex->header.width) + j*2 + 1] = srcTex[bitsOff + m*(bih->biWidth/2) + j] & 0x0f;
            }
        }
    }
	TRACE(L"Texture replaced.");
}

void FreePNGTexture(BITMAPINFO* bitmap) 
{
	if (bitmap != NULL) {
        pngdib_p2d_free_dib(NULL, (BITMAPINFOHEADER*)bitmap);
	}
}

WORD GetTeamIdBySrc(TEAM_KIT_INFO* src)
{
    // check NextMatch areas first
    NEXT_MATCH_DATA_INFO* pNM = *(NEXT_MATCH_DATA_INFO**)data[NEXT_MATCH_DATA_PTR];
    if (pNM)
    {
        if (pNM->home &&  &pNM->home->tki == src)
            return pNM->home->teamId;
        if (pNM->away &&  &pNM->away->tki == src)
            return pNM->away->teamId;
    }

    // check normal area
    TEAM_KIT_INFO* teamKitInfoBase = 
        (TEAM_KIT_INFO*)(*(DWORD*)data[TEAM_KIT_INFO_BASE] + 0xe8bc4);
    WORD teamId = ((DWORD)src - (DWORD)teamKitInfoBase) >> 9;
    if (teamId < NUM_REGULAR_TEAMS)
        return teamId;

    // check special teams area
    TEAM_KIT_INFO* base = 
        (TEAM_KIT_INFO*)(*(DWORD*)data[TEAM_KIT_INFO_BASE] + 0x12a9d0);
    teamId = ((DWORD)src - (DWORD)base) >> 9;
    if (teamId < TOTAL_TEAMS - NUM_ADDITIONAL_TEAMS)
        return teamId + NUM_ADDITIONAL_TEAMS;

    // check additional teams area
    TEAM_KIT_INFO* base2 = 
        (TEAM_KIT_INFO*)(*(DWORD*)data[TEAM_KIT_INFO_BASE] + 0x12e910);
    teamId = ((DWORD)src - (DWORD)base2) >> 9;
    if (teamId < TOTAL_TEAMS - NUM_REGULAR_TEAMS)
        return teamId + NUM_REGULAR_TEAMS;
    
    return 0xffff;
}

TEAM_KIT_INFO* GetTeamKitInfoById(WORD id)
{
    if (id < NUM_REGULAR_TEAMS)
    {
        TEAM_KIT_INFO* teamKitInfoBase = 
            (TEAM_KIT_INFO*)(*(DWORD*)data[TEAM_KIT_INFO_BASE] + 0xe8bc4);
        return &teamKitInfoBase[id];
    }
    else if (id >= NUM_REGULAR_TEAMS && id < NUM_ADDITIONAL_TEAMS)
    {
        TEAM_KIT_INFO* teamKitInfoBase = 
            (TEAM_KIT_INFO*)(*(DWORD*)data[TEAM_KIT_INFO_BASE] + 0x12e910);
        return &teamKitInfoBase[id - NUM_REGULAR_TEAMS];
    }
    else if (id >= NUM_ADDITIONAL_TEAMS && id < TOTAL_TEAMS)
    {
        TEAM_KIT_INFO* teamKitInfoBase = 
            (TEAM_KIT_INFO*)(*(DWORD*)data[TEAM_KIT_INFO_BASE] + 0x12a9d0);
        return &teamKitInfoBase[id - NUM_ADDITIONAL_TEAMS];
    }
    return NULL;
}

KEXPORT void kservAfterReadTeamKitInfo(TEAM_KIT_INFO* dest, TEAM_KIT_INFO* src)
{
    //LOG3N(L"kservAfterReadTeamKitInfo: team = %d, dest = %08x, src = %08x", 
    //        GetTeamIdBySrc(src), (DWORD)dest, (DWORD)src);

    if (!g_slotMapsInitialized)
    {
        InitSlotMap();
        g_slotMapsInitialized = true;
    }

    WORD teamId = GetTeamIdBySrc(src);

    // First, handle the standard permutations
    {
        NEXT_MATCH_DATA_INFO* pNM = *(NEXT_MATCH_DATA_INFO**)data[NEXT_MATCH_DATA_PTR];
        if (pNM && (pNM->home->teamId == teamId || pNM->away->teamId == teamId))
        {
            map<wstring,Kit>::iterator iterPL = (pNM->home->teamId == teamId) ? g_iterHomePL : g_iterAwayPL;
            map<wstring,Kit>::iterator iterGK = (pNM->home->teamId == teamId) ? g_iterHomeGK : g_iterAwayGK;
            map<wstring,Kit>::iterator iterPL_end = (pNM->home->teamId == teamId) ? g_iterHomePL_end : g_iterAwayPL_end;
            map<wstring,Kit>::iterator iterGK_end = (pNM->home->teamId == teamId) ? g_iterHomeGK_end : g_iterAwayGK_end;

            // GK
            if (iterGK != iterGK_end && iterGK->first == L"ga")
            {
                memcpy(&dest->gb, &dest->ga, sizeof(KIT_INFO));
            }
            else if (iterGK != iterGK_end && iterGK->first == L"gb")
            {
                memcpy(&dest->ga, &dest->gb, sizeof(KIT_INFO));
            }
            // PL
            if (iterPL != iterPL_end && iterPL->first == L"pa")
            {
                memcpy(&dest->pb, &dest->pa, sizeof(KIT_INFO));
            }
            else if (iterPL != iterPL_end && iterPL->first == L"pb")
            {
                memcpy(&dest->pa, &dest->pb, sizeof(KIT_INFO));
            }
        }
    }

    // Second, check for attributes from GDB
    hash_map<WORD,KitCollection>::iterator it = gdb->uni.find(teamId);
    if (it != gdb->uni.end())
    {
        // re-link, if necessary
        hash_map<WORD,WORD>::iterator tid = g_kitSlotByTeamId.find(teamId);
        if (tid == g_kitSlotByTeamId.end())
        {
            TRACE1N(L"Re-linking team %d...", teamId);
            EnterCriticalSection(&_cs_slots);
            // this is a team in GDB, but without a kit slot in AFS
            // We need to temporarily re-link it to an available slot.
            WORD kitSlot = FindFreeKitSlot();
            if (kitSlot != 0xffff)
            {
                TRACE2N(L"Team %d now uses slot 0x%04x", teamId, kitSlot);
                g_kitSlotByTeamId.insert(pair<WORD,WORD>(teamId,kitSlot));
                g_teamIdByKitSlot.insert(pair<WORD,WORD>(kitSlot,teamId));
                if (!it->second.goalkeepers.empty())
                {
                    dest->ga.kitLink = kitSlot;
                    dest->gb.kitLink = kitSlot;
                }
                if (!it->second.players.empty())
                {
                    dest->pa.kitLink = kitSlot;
                    dest->pb.kitLink = kitSlot;
                }
            }
            else
            {
                LOG1N(L"Unable to re-link team %d: no slots available.", teamId);
            }
            LeaveCriticalSection(&_cs_slots);
        }
        else
        {
            // set slot
            if (!it->second.goalkeepers.empty())
            {
                dest->ga.kitLink = tid->second;
                dest->gb.kitLink = tid->second;
            }
            if (!it->second.players.empty())
            {
                dest->pa.kitLink = tid->second;
                dest->pb.kitLink = tid->second;
            }
        }

        // apply attributes
        // GK
        NEXT_MATCH_DATA_INFO* pNM = *(NEXT_MATCH_DATA_INFO**)data[NEXT_MATCH_DATA_PTR];
        if (pNM && pNM->home->teamId == teamId)
        {
            if (g_iterHomeGK != g_iterHomeGK_end) 
            {
                ApplyKitAttributes(g_iterHomeGK,dest->ga);
                ApplyKitAttributes(g_iterHomeGK,dest->gb);
            }
            else
            {
                ApplyKitAttributes(it->second.goalkeepers,L"ga",dest->ga);
                ApplyKitAttributes(it->second.goalkeepers,L"gb",dest->gb);
            }
        }
        else if (pNM && pNM->away->teamId == teamId)
        {
            if (g_iterAwayGK != g_iterAwayGK_end) 
            {
                ApplyKitAttributes(g_iterAwayGK,dest->ga);
                ApplyKitAttributes(g_iterAwayGK,dest->gb);
            }
            else
            {
                ApplyKitAttributes(it->second.goalkeepers,L"ga",dest->ga);
                ApplyKitAttributes(it->second.goalkeepers,L"gb",dest->gb);
            }
        }
        else
        {
            ApplyKitAttributes(it->second.goalkeepers,L"ga",dest->ga);
            ApplyKitAttributes(it->second.goalkeepers,L"gb",dest->gb);
        }
        // PL
        if (pNM && pNM->home->teamId == teamId)
        {
            if (g_iterHomePL != g_iterHomePL_end)
            {
                ApplyKitAttributes(g_iterHomePL,dest->pa);
                ApplyKitAttributes(g_iterHomePL,dest->pb);
            }
            else 
            {
                ApplyKitAttributes(it->second.players,L"pa",dest->pa);
                ApplyKitAttributes(it->second.players,L"pb",dest->pb);
            }
        }
        else if (pNM && pNM->away->teamId == teamId)
        {
            if (g_iterAwayPL != g_iterAwayPL_end)
            {
                ApplyKitAttributes(g_iterAwayPL,dest->pa);
                ApplyKitAttributes(g_iterAwayPL,dest->pb);
            }
            else
            {
                ApplyKitAttributes(it->second.players,L"pa",dest->pa);
                ApplyKitAttributes(it->second.players,L"pb",dest->pb);
            }
        }
        else
        {
            ApplyKitAttributes(it->second.players,L"pa",dest->pa);
            ApplyKitAttributes(it->second.players,L"pb",dest->pb);
        }
    }
}

void ApplyKitAttributes(map<wstring,Kit>& m, const wchar_t* kitKey, KIT_INFO& ki)
{
    map<wstring,Kit>::iterator kiter = m.find(kitKey);
    if (kiter != m.end())
        ApplyKitAttributes(kiter, ki);
}

KEXPORT void ApplyKitAttributes(const map<wstring,Kit>::iterator kiter, KIT_INFO& ki)
{
    // load kit attributes from config.txt, if needed
    gdb->loadConfig(kiter->second);

    //LOG1S(L"applying attributes for {%s}", kiter->first.c_str());
    
    // apply attributes
    if (kiter->second.attDefined & MODEL)
        ki.model = kiter->second.model;
    if (kiter->second.attDefined & COLLAR)
        ki.collar = kiter->second.collar;
    if (kiter->second.attDefined & SHIRT_NUMBER_LOCATION)
        ki.frontNumberPosition = kiter->second.shirtNumberLocation;
    if (kiter->second.attDefined & SHORTS_NUMBER_LOCATION)
        ki.shortsNumberPosition = kiter->second.shortsNumberLocation;
    if (kiter->second.attDefined & NAME_LOCATION)
        ki.fontPosition = kiter->second.nameLocation;
    if (kiter->second.attDefined & NAME_SHAPE)
        ki.fontShape = kiter->second.nameShape;
    //if (kiter->second.attDefined & LOGO_LOCATION)
    //    ki.logoLocation = kiter->second.logoLocation;
    if (kiter->second.attDefined & MAIN_COLOR) 
    {
        RGBAColor2KCOLOR(kiter->second.mainColor, ki.mainColor);
        // kit selection uses all 5 shirt colors - not only main (first one)
        for (int i=0; i<4; i++)
            RGBAColor2KCOLOR(kiter->second.mainColor, ki.editShirtColors[i]);
    }
    // shorts main color
    if (kiter->second.attDefined & SHORTS_MAIN_COLOR)
        RGBAColor2KCOLOR(kiter->second.shortsFirstColor, ki.shortsFirstColor);
}

void RGBAColor2KCOLOR(const RGBAColor& color, KCOLOR& kcolor)
{
    kcolor = 0x8000
        +((color.r>>3) & 31)
        +0x20*((color.g>>3) & 31)
        +0x400*((color.b>>3) & 31);
}

void KCOLOR2RGBAColor(const KCOLOR kcolor, RGBAColor& color)
{
    color.r = (kcolor & 31)<<3;
    color.g = (kcolor>>5 & 31)<<3;
    color.b = (kcolor>>10 & 31)<<3;
    color.a = 0xff;
}

int GetBinType(DWORD id)
{
    if (id >= FIRST_KIT_BIN && id < FIRST_KIT_BIN + NUM_SLOTS*2)
    {
        return BIN_KIT_GK + ((id - FIRST_KIT_BIN)%2);
    }
    else if (id >= FIRST_FONT_BIN && id < FIRST_FONT_BIN + NUM_SLOTS*4)
    {
        return BIN_FONT_GA + ((id - FIRST_FONT_BIN)%4);
    }
    else if (id >= FIRST_NUMS_BIN && id < FIRST_NUMS_BIN + NUM_SLOTS*4)
    {
        return BIN_NUMS_GA + ((id - FIRST_NUMS_BIN)%4);
    }

    // special logic for South Korea kits in 1.20
    if (id == SOUTH_KOREA_GK)
        return BIN_KIT_GK;
    else if (id == SOUTH_KOREA_PL)
        return BIN_KIT_PL;

    return -1;
}

int GetKitSlot(DWORD id)
{
    if (id >= FIRST_KIT_BIN && id < FIRST_KIT_BIN + NUM_SLOTS*2)
    {
        return (id - FIRST_KIT_BIN)/2;
    }
    else if (id >= FIRST_FONT_BIN && id < FIRST_FONT_BIN + NUM_SLOTS*4)
    {
        return (id - FIRST_FONT_BIN)/4;
    }
    else if (id >= FIRST_NUMS_BIN && id < FIRST_NUMS_BIN + NUM_SLOTS*4)
    {
        return (id - FIRST_NUMS_BIN)/4;
    }

    // special logic for South Korea kits in 1.20
    if (id == SOUTH_KOREA_GK)
        return (SOUTH_KOREA_GK_CV0 - FIRST_KIT_BIN)/2;
    else if (id == SOUTH_KOREA_PL)
        return (SOUTH_KOREA_PL_CV0 - FIRST_KIT_BIN)/2;

    return -1;
}

WORD FindFreeKitSlot()
{
    for (WORD i=0; i<NUM_SLOTS; i++)
    {
        hash_map<WORD,WORD>::iterator tid = g_teamIdByKitSlot.find(i);
        if (tid == g_teamIdByKitSlot.end())
        {
            //LeaveCriticalSection(&_cs_slots);
            return i;
        }
        TRACE2N(L"FindFreeKitSlot: slot %04x is taken, by teamId = %d",tid->first,tid->second);
    }
    return -1;
}

void kservKeyboardEvent(int code1, WPARAM wParam, LPARAM lParam)
{
    if (getOverlayPage() != _myPage)
        return;

	if (code1 >= 0 && code1==HC_ACTION && lParam & 0x80000000) {
        if (wParam == 0x31) { // home PL
            if (g_iterHomePL == g_iterHomePL_end)
                g_iterHomePL = g_iterHomePL_begin;
            else
                g_iterHomePL++;
        }
        else if (wParam == 0x32) { // away PL
            if (g_iterAwayPL == g_iterAwayPL_end)
                g_iterAwayPL = g_iterAwayPL_begin;
            else
                g_iterAwayPL++;
        }
        else if (wParam == 0x33) { // home GK
            if (g_iterHomeGK == g_iterHomeGK_end)
                g_iterHomeGK = g_iterHomeGK_begin;
            else
                g_iterHomeGK++;
        }
        else if (wParam == 0x34) { // away GK
            if (g_iterAwayGK == g_iterAwayGK_end)
                g_iterAwayGK = g_iterAwayGK_begin;
            else
                g_iterAwayGK++;
        }
    }	
}

void ResetIterators()
{
    // reset kit iterators
    NEXT_MATCH_DATA_INFO* pNM = *(NEXT_MATCH_DATA_INFO**)data[NEXT_MATCH_DATA_PTR];
    g_iterHomePL_begin = gdb->dummyHome.players.begin();
    g_iterHomeGK_begin = gdb->dummyHome.goalkeepers.begin();
    g_iterHomePL_end = gdb->dummyHome.players.end();
    g_iterHomeGK_end = gdb->dummyHome.goalkeepers.end();

    g_iterAwayPL_begin = gdb->dummyAway.players.begin();
    g_iterAwayGK_begin = gdb->dummyAway.goalkeepers.begin();
    g_iterAwayPL_end = gdb->dummyAway.players.end();
    g_iterAwayGK_end = gdb->dummyAway.goalkeepers.end();

    if (pNM && pNM->home)
    {
        g_lastHome = pNM->home->teamId;
        TRACE1N(L"ResetIterators: home team: %d", pNM->home->teamId);
        hash_map<WORD,KitCollection>::iterator it = gdb->uni.find(pNM->home->teamId);
        if (it != gdb->uni.end())
        {
            if (it->second.players.begin() != it->second.players.end())
            {
                g_iterHomePL_begin = it->second.players.begin();
                g_iterHomePL_end = it->second.players.end();
            }
            // otherwise: leave them as dummy iterators: this way we can still
            // choose a/b, even if no kits are in GDB folder
            if (it->second.goalkeepers.begin() != it->second.goalkeepers.end())
            {
                g_iterHomeGK_begin = it->second.goalkeepers.begin();
                g_iterHomeGK_end = it->second.goalkeepers.end();
            }
            // otherwise: leave them as dummy iterators: this way we can still
            // choose a/b, even if no kits are in GDB folder
        }
    }
    if (pNM && pNM->away)
    {
        g_lastAway = pNM->away->teamId;
        TRACE1N(L"ResetIterators: away team: %d", pNM->away->teamId);
        hash_map<WORD,KitCollection>::iterator it = gdb->uni.find(pNM->away->teamId);
        if (it != gdb->uni.end())
        {
            if (it->second.players.begin() != it->second.players.end())
            {
                g_iterAwayPL_begin = it->second.players.begin();
                g_iterAwayPL_end = it->second.players.end();
            }
            // otherwise: leave them as dummy iterators: this way we can still
            // choose a/b, even if no kits are in GDB folder
            if (it->second.goalkeepers.begin() != it->second.goalkeepers.end())
            {
                g_iterAwayGK_begin = it->second.goalkeepers.begin();
                g_iterAwayGK_end = it->second.goalkeepers.end();
            }
            // otherwise: leave them as dummy iterators: this way we can still
            // choose a/b, even if no kits are in GDB folder
        }
    }

    g_iterHomePL = g_iterHomePL_end;
    g_iterHomeGK = g_iterHomeGK_end;
    g_iterAwayPL = g_iterAwayPL_end;
    g_iterAwayGK = g_iterAwayGK_end;

    // reset kit-enforcers
    _home_forced = false;
    _away_forced = false;

    TRACE(L"Iterators reset");
}

void GetCurrentTeams(WORD& home, WORD& away)
{
    NEXT_MATCH_DATA_INFO* pNM = *(NEXT_MATCH_DATA_INFO**)data[NEXT_MATCH_DATA_PTR];
    if (pNM && pNM->home) home = pNM->home->teamId;
    if (pNM && pNM->away) away = pNM->away->teamId;
}

void kservPresent(IDirect3DDevice9* self, CONST RECT* src, CONST RECT* dest,
	HWND hWnd, LPVOID unused)
{
    if (getOverlayPage() != _myPage)  // page-check
        return;

    if (g_beginShowKitSelection)
    {
        // may need to reset iterators, if the teams changed since last time
        WORD home = 0xffff, away = 0xffff;
        GetCurrentTeams(home,away);
        if (home == 0xffff || away == 0xffff || home != g_lastHome || away != g_lastAway)
            ResetIterators(); // new teams, therefore need a reset

        g_lastHome = home;
        g_lastAway = away;
        g_beginShowKitSelection = false;
    }

    /*
	wchar_t* rp = Utf8::ansiToUnicode("kservPresent");
	KDrawText(rp, 0, 0, D3DCOLOR_RGBA(255,0,0,192));
	Utf8::free(rp);
    */
	//KDrawText(L"kservPresent", 0, 0, D3DCOLOR_RGBA(0xff,0xff,0xff,0xff), 20.0f);

    NEXT_MATCH_DATA_INFO* pNM = *(NEXT_MATCH_DATA_INFO**)data[NEXT_MATCH_DATA_PTR];
    // safety checks
    if (!pNM || !pNM->home || !pNM->away)
        return;
    if (pNM->home->teamId == 0xffff || pNM->away->teamId == 0xffff)
        return;

    char buf[20] = {0};
    sprintf(buf, "%d vs %d", pNM->home->teamId, pNM->away->teamId);
	wchar_t* rp = Utf8::ansiToUnicode(buf);
	KDrawText(rp, 7, 7, COLOR_BLACK, 26.0f);
	KDrawText(rp, 5, 5, COLOR_INFO, 26.0f);
	Utf8::free(rp);

    // display "home"/"away" helper for Master League games
    if (pNM->home->teamIdSpecial != pNM->home->teamId)
    {
        KDrawText(L"Home", 7, 35, COLOR_BLACK, 26.0f);
        KDrawText(L"Home", 5, 33, COLOR_INFO, 26.0f);
    }
    else if (pNM->away->teamIdSpecial != pNM->away->teamId)
    {
        KDrawText(L"Away", 7, 35, COLOR_BLACK, 26.0f);
        KDrawText(L"Away", 5, 33, COLOR_INFO, 26.0f);
    }

    // display current kit selection

    // Home PL
    if (g_iterHomePL == g_iterHomePL_end)
    {
        KDrawText(L"P: auto", 200, 7, COLOR_BLACK, 30.0f);
        KDrawText(L"P: auto", 202, 5, COLOR_BLACK, 30.0f);
        KDrawText(L"P: auto", 202, 7, COLOR_BLACK, 30.0f);
        KDrawText(L"P: auto", 200, 5, COLOR_AUTO, 30.0f);
    }
    else
    {
        wchar_t wbuf[512] = {0};
        if (_kserv_config._use_description && !g_iterHomePL->second.description.empty())
            swprintf(wbuf, L"P: %s", g_iterHomePL->second.description.c_str());
        else
            swprintf(wbuf, L"P: %s", g_iterHomePL->first.c_str());
        KDrawText(wbuf, 200, 7, COLOR_BLACK, 30.0f);
        KDrawText(wbuf, 202, 5, COLOR_BLACK, 30.0f);
        KDrawText(wbuf, 202, 7, COLOR_BLACK, 30.0f);
        KDrawText(wbuf, 200, 5, COLOR_CHOSEN, 30.0f);
        gdb->loadConfig(g_iterHomePL->second);
        if (g_iterHomePL->second.attDefined & MAIN_COLOR)
        {
            RGBAColor& c = g_iterHomePL->second.mainColor;
            KDrawText(L"\x2580", 180, 7, COLOR_BLACK, 26.0f, KDT_BOLD);
            KDrawText(L"\x2580", 178, 5, D3DCOLOR_RGBA(c.r,c.g,c.b,c.a), 26.0f, KDT_BOLD);
        }
        else if (g_iterHomePL->second.foldername == L"")
        {
            // non-GDB kit => get main color from attribute structures
            NEXT_MATCH_DATA_INFO* pNM = *(NEXT_MATCH_DATA_INFO**)data[NEXT_MATCH_DATA_PTR];
            TEAM_KIT_INFO* tki = GetTeamKitInfoById(pNM->home->teamId);
            if (pNM->home->teamIdSpecial != pNM->home->teamId)
                tki = &pNM->home->tki;

            KCOLOR kc = (g_iterHomePL->first == L"pa") ? tki->pa.mainColor : tki->pb.mainColor;
            RGBAColor c;
            KCOLOR2RGBAColor(kc, c);
            KDrawText(L"\x2580", 180, 7, COLOR_BLACK, 26.0f, KDT_BOLD);
            KDrawText(L"\x2580", 178, 5, D3DCOLOR_RGBA(c.r,c.g,c.b,c.a), 26.0f, KDT_BOLD);
        }
        if (g_iterHomePL->second.attDefined & SHORTS_MAIN_COLOR)
        {
            RGBAColor& c = g_iterHomePL->second.shortsFirstColor;
            KDrawText(L"\x2584", 180, 7, COLOR_BLACK, 26.0f, KDT_BOLD);
            KDrawText(L"\x2584", 178, 5, D3DCOLOR_RGBA(c.r,c.g,c.b,c.a), 26.0f, KDT_BOLD);
        }
        else if (g_iterHomePL->second.foldername == L"")
        {
            // non-GDB kit => get shorts color from attribute structures
            NEXT_MATCH_DATA_INFO* pNM = *(NEXT_MATCH_DATA_INFO**)data[NEXT_MATCH_DATA_PTR];
            TEAM_KIT_INFO* tki = GetTeamKitInfoById(pNM->home->teamId);
            if (pNM->home->teamIdSpecial != pNM->home->teamId)
                tki = &pNM->home->tki;

            KCOLOR kc = (g_iterHomePL->first == L"pa") ? tki->pa.shortsFirstColor : tki->pb.shortsFirstColor;
            RGBAColor c;
            KCOLOR2RGBAColor(kc, c);
            KDrawText(L"\x2584", 180, 7, COLOR_BLACK, 26.0f, KDT_BOLD);
            KDrawText(L"\x2584", 178, 5, D3DCOLOR_RGBA(c.r,c.g,c.b,c.a), 26.0f, KDT_BOLD);
        }
    }
    // Home GK
    if (g_iterHomeGK == g_iterHomeGK_end)
    {
        KDrawText(L"G: auto", 200, 35, COLOR_BLACK, 30.0f);
        KDrawText(L"G: auto", 202, 33, COLOR_BLACK, 30.0f);
        KDrawText(L"G: auto", 202, 35, COLOR_BLACK, 30.0f);
        KDrawText(L"G: auto", 200, 33, COLOR_AUTO, 30.0f);
    }
    else
    {
        wchar_t wbuf[512] = {0};
        if (_kserv_config._use_description && !g_iterHomeGK->second.description.empty())
            swprintf(wbuf, L"G: %s", g_iterHomeGK->second.description.c_str());
        else
            swprintf(wbuf, L"G: %s", g_iterHomeGK->first.c_str());
        KDrawText(wbuf, 200, 35, COLOR_BLACK, 30.0f);
        KDrawText(wbuf, 202, 33, COLOR_BLACK, 30.0f);
        KDrawText(wbuf, 202, 35, COLOR_BLACK, 30.0f);
        KDrawText(wbuf, 200, 33, COLOR_CHOSEN, 30.0f);
        gdb->loadConfig(g_iterHomeGK->second);
        if (g_iterHomeGK->second.attDefined & MAIN_COLOR)
        {
            RGBAColor& c = g_iterHomeGK->second.mainColor;
            KDrawText(L"\x2580", 180, 35, COLOR_BLACK, 26.0f, KDT_BOLD);
            KDrawText(L"\x2580", 178, 33, D3DCOLOR_RGBA(c.r,c.g,c.b,c.a), 26.0f, KDT_BOLD);
        }
        else if (g_iterHomeGK->second.foldername == L"")
        {
            // non-GDB kit => get main color from attribute structures
            NEXT_MATCH_DATA_INFO* pNM = *(NEXT_MATCH_DATA_INFO**)data[NEXT_MATCH_DATA_PTR];
            TEAM_KIT_INFO* tki = GetTeamKitInfoById(pNM->home->teamId);
            if (pNM->home->teamIdSpecial != pNM->home->teamId)
                tki = &pNM->home->tki;

            KCOLOR kc = (g_iterHomeGK->first == L"ga") ? tki->ga.mainColor : tki->gb.mainColor;
            RGBAColor c;
            KCOLOR2RGBAColor(kc, c);
            KDrawText(L"\x2580", 180, 35, COLOR_BLACK, 26.0f, KDT_BOLD);
            KDrawText(L"\x2580", 178, 33, D3DCOLOR_RGBA(c.r,c.g,c.b,c.a), 26.0f, KDT_BOLD);
        }
        if (g_iterHomeGK->second.attDefined & SHORTS_MAIN_COLOR)
        {
            RGBAColor& c = g_iterHomeGK->second.shortsFirstColor;
            KDrawText(L"\x2584", 180, 35, COLOR_BLACK, 26.0f, KDT_BOLD);
            KDrawText(L"\x2584", 178, 33, D3DCOLOR_RGBA(c.r,c.g,c.b,c.a), 26.0f, KDT_BOLD);
        }
        else if (g_iterHomeGK->second.foldername == L"")
        {
            // non-GDB kit => get shorts color from attribute structures
            NEXT_MATCH_DATA_INFO* pNM = *(NEXT_MATCH_DATA_INFO**)data[NEXT_MATCH_DATA_PTR];
            TEAM_KIT_INFO* tki = GetTeamKitInfoById(pNM->home->teamId);
            if (pNM->home->teamIdSpecial != pNM->home->teamId)
                tki = &pNM->home->tki;

            KCOLOR kc = (g_iterHomeGK->first == L"ga") ? tki->ga.shortsFirstColor : tki->gb.shortsFirstColor;
            RGBAColor c;
            KCOLOR2RGBAColor(kc, c);
            KDrawText(L"\x2584", 180, 35, COLOR_BLACK, 26.0f, KDT_BOLD);
            KDrawText(L"\x2584", 178, 33, D3DCOLOR_RGBA(c.r,c.g,c.b,c.a), 26.0f, KDT_BOLD);
        }
    }
    // Away PL
    if (g_iterAwayPL == g_iterAwayPL_end)
    {
        KDrawText(L"P: auto", 600, 7, COLOR_BLACK, 30.0f);
        KDrawText(L"P: auto", 602, 5, COLOR_BLACK, 30.0f);
        KDrawText(L"P: auto", 602, 7, COLOR_BLACK, 30.0f);
        KDrawText(L"P: auto", 600, 5, COLOR_AUTO, 30.0f);
    }
    else
    {
        wchar_t wbuf[512] = {0};
        if (_kserv_config._use_description && !g_iterAwayPL->second.description.empty())
            swprintf(wbuf, L"P: %s", g_iterAwayPL->second.description.c_str());
        else
            swprintf(wbuf, L"P: %s", g_iterAwayPL->first.c_str());
        KDrawText(wbuf, 600, 7, COLOR_BLACK, 30.0f);
        KDrawText(wbuf, 602, 5, COLOR_BLACK, 30.0f);
        KDrawText(wbuf, 602, 7, COLOR_BLACK, 30.0f);
        KDrawText(wbuf, 600, 5, COLOR_CHOSEN, 30.0f);
        gdb->loadConfig(g_iterAwayPL->second);
        if (g_iterAwayPL->second.attDefined & MAIN_COLOR)
        {
            RGBAColor& c = g_iterAwayPL->second.mainColor;
            KDrawText(L"\x2580", 580, 7, COLOR_BLACK, 26.0f, KDT_BOLD);
            KDrawText(L"\x2580", 578, 5, D3DCOLOR_RGBA(c.r,c.g,c.b,c.a), 26.0f, KDT_BOLD);
        }
        else if (g_iterAwayPL->second.foldername == L"")
        {
            // non-GDB kit => get main color from attribute structures
            NEXT_MATCH_DATA_INFO* pNM = *(NEXT_MATCH_DATA_INFO**)data[NEXT_MATCH_DATA_PTR];
            TEAM_KIT_INFO* tki = GetTeamKitInfoById(pNM->away->teamId);
            if (pNM->away->teamIdSpecial != pNM->away->teamId)
                tki = &pNM->away->tki;

            KCOLOR kc = (g_iterAwayPL->first == L"pa") ? tki->pa.mainColor : tki->pb.mainColor;
            RGBAColor c;
            KCOLOR2RGBAColor(kc, c);
            KDrawText(L"\x2580", 580, 7, COLOR_BLACK, 26.0f, KDT_BOLD);
            KDrawText(L"\x2580", 578, 5, D3DCOLOR_RGBA(c.r,c.g,c.b,c.a), 26.0f, KDT_BOLD);
        }
        if (g_iterAwayPL->second.attDefined & SHORTS_MAIN_COLOR)
        {
            RGBAColor& c = g_iterAwayPL->second.shortsFirstColor;
            KDrawText(L"\x2584", 580, 7, COLOR_BLACK, 26.0f, KDT_BOLD);
            KDrawText(L"\x2584", 578, 5, D3DCOLOR_RGBA(c.r,c.g,c.b,c.a), 26.0f, KDT_BOLD);
        }
        else if (g_iterAwayPL->second.foldername == L"")
        {
            // non-GDB kit => get shorts color from attribute structures
            NEXT_MATCH_DATA_INFO* pNM = *(NEXT_MATCH_DATA_INFO**)data[NEXT_MATCH_DATA_PTR];
            TEAM_KIT_INFO* tki = GetTeamKitInfoById(pNM->away->teamId);
            if (pNM->away->teamIdSpecial != pNM->away->teamId)
                tki = &pNM->away->tki;

            KCOLOR kc = (g_iterAwayPL->first == L"pa") ? tki->pa.shortsFirstColor : tki->pb.shortsFirstColor;
            RGBAColor c;
            KCOLOR2RGBAColor(kc, c);
            KDrawText(L"\x2584", 580, 7, COLOR_BLACK, 26.0f, KDT_BOLD);
            KDrawText(L"\x2584", 578, 5, D3DCOLOR_RGBA(c.r,c.g,c.b,c.a), 26.0f, KDT_BOLD);
        }

    }
    // Away GK
    if (g_iterAwayGK == g_iterAwayGK_end)
    {
        KDrawText(L"G: auto", 600, 35, COLOR_BLACK, 30.0f);
        KDrawText(L"G: auto", 602, 33, COLOR_BLACK, 30.0f);
        KDrawText(L"G: auto", 602, 35, COLOR_BLACK, 30.0f);
        KDrawText(L"G: auto", 600, 33, COLOR_AUTO, 30.0f);
    }
    else
    {
        wchar_t wbuf[512] = {0};
        if (_kserv_config._use_description && !g_iterAwayGK->second.description.empty())
            swprintf(wbuf, L"G: %s", g_iterAwayGK->second.description.c_str());
        else
            swprintf(wbuf, L"G: %s", g_iterAwayGK->first.c_str());
        KDrawText(wbuf, 600, 35, COLOR_BLACK, 30.0f);
        KDrawText(wbuf, 602, 33, COLOR_BLACK, 30.0f);
        KDrawText(wbuf, 602, 35, COLOR_BLACK, 30.0f);
        KDrawText(wbuf, 600, 33, COLOR_CHOSEN, 30.0f);
        gdb->loadConfig(g_iterAwayGK->second);
        if (g_iterAwayGK->second.attDefined & MAIN_COLOR)
        {
            RGBAColor& c = g_iterAwayGK->second.mainColor;
            KDrawText(L"\x2580", 580, 35, COLOR_BLACK, 26.0f, KDT_BOLD);
            KDrawText(L"\x2580", 578, 33, D3DCOLOR_RGBA(c.r,c.g,c.b,c.a), 26.0f, KDT_BOLD);
        }
        else if (g_iterAwayGK->second.foldername == L"")
        {
            // non-GDB kit => get main color from attribute structures
            NEXT_MATCH_DATA_INFO* pNM = *(NEXT_MATCH_DATA_INFO**)data[NEXT_MATCH_DATA_PTR];
            TEAM_KIT_INFO* tki = GetTeamKitInfoById(pNM->away->teamId);
            if (pNM->away->teamIdSpecial != pNM->away->teamId)
                tki = &pNM->away->tki;

            KCOLOR kc = (g_iterAwayGK->first == L"ga") ? tki->ga.mainColor : tki->gb.mainColor;
            RGBAColor c;
            KCOLOR2RGBAColor(kc, c);
            KDrawText(L"\x2580", 580, 35, COLOR_BLACK, 26.0f, KDT_BOLD);
            KDrawText(L"\x2580", 578, 33, D3DCOLOR_RGBA(c.r,c.g,c.b,c.a), 26.0f, KDT_BOLD);
        }
        if (g_iterAwayGK->second.attDefined & SHORTS_MAIN_COLOR)
        {
            RGBAColor& c = g_iterAwayGK->second.shortsFirstColor;
            KDrawText(L"\x2584", 580, 35, COLOR_BLACK, 26.0f, KDT_BOLD);
            KDrawText(L"\x2584", 578, 33, D3DCOLOR_RGBA(c.r,c.g,c.b,c.a), 26.0f, KDT_BOLD);
        }
        else if (g_iterAwayGK->second.foldername == L"")
        {
            // non-GDB kit => get shorts color from attribute structures
            NEXT_MATCH_DATA_INFO* pNM = *(NEXT_MATCH_DATA_INFO**)data[NEXT_MATCH_DATA_PTR];
            TEAM_KIT_INFO* tki = GetTeamKitInfoById(pNM->away->teamId);
            if (pNM->away->teamIdSpecial != pNM->away->teamId)
                tki = &pNM->away->tki;

            KCOLOR kc = (g_iterAwayGK->first == L"ga") ? tki->ga.shortsFirstColor : tki->gb.shortsFirstColor;
            RGBAColor c;
            KCOLOR2RGBAColor(kc, c);
            KDrawText(L"\x2584", 580, 35, COLOR_BLACK, 26.0f, KDT_BOLD);
            KDrawText(L"\x2584", 578, 33, D3DCOLOR_RGBA(c.r,c.g,c.b,c.a), 26.0f, KDT_BOLD);
        }
    }
}

void kservOverlayEvent(bool overlayOn, bool isExhibitionMode, int delta, DWORD menuMode)
{
    if (overlayOn && !g_presentHooked)
    {
        if (isExhibitionMode && delta==1)
            ResetIterators();

        hookFunction(hk_D3D_Present, kservPresent);
        setOverlayPageVisible(_myPage, true);
        g_presentHooked = true;
        g_beginShowKitSelection = true;
    }
    else if (!overlayOn && g_presentHooked)
    {
        if (isExhibitionMode && delta==-1)
            ResetIterators();

        setOverlayPageVisible(_myPage, false);
        unhookFunction(hk_D3D_Present, kservPresent);
        g_presentHooked = false;
    }
}

void SetAttributes(TEAM_KIT_INFO* src, WORD teamId)
{
    // save orginals
    TEAM_KIT_INFO tki;
    memcpy(&tki, src, sizeof(TEAM_KIT_INFO));
    EnterCriticalSection(&_cs_savedTki);
    hash_map<DWORD,TEAM_KIT_INFO>::iterator it = g_savedTki.find((DWORD)src);
    if (it == g_savedTki.end()) 
    {
        // not found: save
        g_savedTki.insert(pair<DWORD,TEAM_KIT_INFO>((DWORD)src,tki));
        TRACE1N(L"Saved attributes for %08x", (DWORD)src);
    }
    LeaveCriticalSection(&_cs_savedTki);
    kservAfterReadTeamKitInfo(src, src);
}

void RestoreAttributes()
{
    // restore originals
    EnterCriticalSection(&_cs_savedTki);
    for (hash_map<DWORD,TEAM_KIT_INFO>::iterator it = g_savedTki.begin();
            it != g_savedTki.end();
            it++)
    {
        memcpy((BYTE*)it->first, &it->second, sizeof(TEAM_KIT_INFO));
        TRACE1N(L"Restored attributes for %08x", (DWORD)it->first);
    }
    g_savedTki.clear();
    LeaveCriticalSection(&_cs_savedTki);
}

void kservStartReadKitInfo(TEAM_KIT_INFO* src)
{
    WORD teamId = GetTeamIdBySrc(src);
    TRACE2N(L"Reading kit info teamId = %d (src = %08x)",teamId,(DWORD)src);

    // set attributes
    SetAttributes(src, teamId);
}

void kservEndReadKitInfo()
{
    // restore previous
    RestoreAttributes();
}

void kservOnEnterCups()
{
    // need to make sure iterators are reset
    TRACE(L"Entering cup/league/ml mode");
    ResetIterators();
}

void kservOnLeaveCups()
{
    // need to make sure iterators are reset
    TRACE(L"Exiting cup/league/ml mode");
    ResetIterators();
}

/*
void kservAtWriteTeamIdCallPoint()
{
    __asm 
    {
        cmp di,0x12b                  // execute replaced code
        pushfd
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push esi  // parameter: address
        call kservAtWriteTeamId
        add esp,4     // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        retn
    }
}

KEXPORT void kservAtWriteTeamId(TEAM_MATCH_DATA_INFO* addr)
{
    NEXT_MATCH_DATA_INFO* pNM = *(NEXT_MATCH_DATA_INFO**)data[NEXT_MATCH_DATA_PTR];
    if (pNM && pNM->away && addr == pNM->away)
    {
        TRACE(L"Teams written.");
    }
}
*/

/**
 * data read callback
 */
void kservReadReplayData(LPCVOID data, DWORD size)
{
    // read kit keys, if available
    string homeKeyPL((char*)data+0x377de0);
    string homeKeyGK((char*)data+0x377e10);
    string awayKeyPL((char*)data+0x377ed0);
    string awayKeyGK((char*)data+0x377f00);

    WORD homeTeamId = *(WORD*)((BYTE*)data+0x128);
    WORD awayTeamId = *(WORD*)((BYTE*)data+0x174);
    LOG1N(L"homeTeamId = %d", homeTeamId);
    LOG1N(L"awayTeamId = %d", awayTeamId);

    /*
    if (!g_slotMapsInitialized)
    {
        InitSlotMap();
        g_slotMapsInitialized = true;
    }
    */

    hash_map<WORD,KitCollection>::iterator it;
    it = gdb->uni.find(homeTeamId);
    if (it != gdb->uni.end())
    {
        if (!homeKeyPL.empty())
        {
            wchar_t* key = Utf8::ansiToUnicode(homeKeyPL.c_str());
            TRACE1S(L"key = {%s}",key);
            map<wstring,Kit>::iterator kit = it->second.players.find(key);
            if (kit != it->second.players.end())
            {
                g_iterHomePL = kit;
                g_iterHomePL_begin = it->second.players.begin();
                g_iterHomePL_end = it->second.players.end();
            }
            Utf8::free(key);
        }
        if (!homeKeyGK.empty())
        {
            wchar_t* key = Utf8::ansiToUnicode(homeKeyGK.c_str());
            TRACE1S(L"key = {%s}",key);
            map<wstring,Kit>::iterator kit = it->second.goalkeepers.find(key);
            if (kit != it->second.goalkeepers.end())
            {
                g_iterHomeGK = kit;
                g_iterHomeGK_begin = it->second.goalkeepers.begin();
                g_iterHomeGK_end = it->second.goalkeepers.end();
            }
            Utf8::free(key);
        }
    }

    it = gdb->uni.find(awayTeamId);
    if (it != gdb->uni.end())
    {
        if (!awayKeyPL.empty())
        {
            wchar_t* key = Utf8::ansiToUnicode(awayKeyPL.c_str());
            TRACE1S(L"key = {%s}",key);
            map<wstring,Kit>::iterator kit = it->second.players.find(key);
            if (kit != it->second.players.end())
            {
                g_iterAwayPL = kit;
                g_iterAwayPL_begin = it->second.players.begin();
                g_iterAwayPL_end = it->second.players.end();
            }
            Utf8::free(key);
        }
        if (!awayKeyGK.empty())
        {
            wchar_t* key = Utf8::ansiToUnicode(awayKeyGK.c_str());
            TRACE1S(L"key = {%s}",key);
            map<wstring,Kit>::iterator kit = it->second.goalkeepers.find(key);
            if (kit != it->second.goalkeepers.end())
            {
                g_iterAwayGK = kit;
                g_iterAwayGK_begin = it->second.goalkeepers.begin();
                g_iterAwayGK_end = it->second.goalkeepers.end();
            }
            Utf8::free(key);
        }
    }

    //memset((char*)data+0x377de0, 0, 0x30);
    //memset((char*)data+0x377e10, 0, 0x30);
    //memset((char*)data+0x377ed0, 0, 0x30);
    //memset((char*)data+0x377f00, 0, 0x30);
}

/**
 * data write callback
 */
void kservWriteReplayData(LPCVOID data, DWORD size)
{
    // save kit keys
    if (g_iterHomePL != g_iterHomePL_end)
    {
        LOG1S(L"home PL: %s", g_iterHomePL->first.c_str());
        char* key = Utf8::unicodeToAnsi(g_iterHomePL->first.c_str());
        strncpy((char*)data+0x377de0, key, 0x30);
        Utf8::free(key);
    }
    if (g_iterHomeGK != g_iterHomeGK_end)
    {
        LOG1S(L"home GK: %s", g_iterHomeGK->first.c_str());
        char* key = Utf8::unicodeToAnsi(g_iterHomeGK->first.c_str());
        strncpy((char*)data+0x377e10, key, 0x30);
        Utf8::free(key);
    }
    if (g_iterAwayPL != g_iterAwayPL_end)
    {
        LOG1S(L"away PL: %s", g_iterAwayPL->first.c_str());
        char* key = Utf8::unicodeToAnsi(g_iterAwayPL->first.c_str());
        strncpy((char*)data+0x377ed0, key, 0x30);
        Utf8::free(key);
    }
    if (g_iterAwayGK != g_iterAwayGK_end)
    {
        LOG1S(L"away GK: %s", g_iterAwayGK->first.c_str());
        char* key = Utf8::unicodeToAnsi(g_iterAwayGK->first.c_str());
        strncpy((char*)data+0x377f00, key, 0x30);
        Utf8::free(key);
    }
}

void kservAtReadKitSelectionCallPoint()
{
    __asm {
        pushfd 
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push edi // param: TEAM_MATCH_DATA_INFO
        call kservAtReadKitSelection
        add esp,4     // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        mov dl,byte ptr ds:[edi+0x2d38] // execute replaced code
        retn
    }
}

void kservAtReadKitSelectionCallPoint2()
{
    __asm {
        pushfd 
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push eax // param: TEAM_MATCH_DATA_INFO
        call kservAtReadKitSelection
        add esp,4     // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        mov dl,byte ptr ds:[eax+0x2d38] // execute replaced code
        retn
    }
}

KEXPORT void kservAtReadKitSelection(TEAM_MATCH_DATA_INFO* tmdi)
{
    DWORD* cupModePtr = *(DWORD**)data[CUP_MODE_PTR];
    if (cupModePtr)
        return;  // don't need reloads in cup modes

    NEXT_MATCH_DATA_INFO* pNM = 
        *(NEXT_MATCH_DATA_INFO**)data[NEXT_MATCH_DATA_PTR];
    if (pNM && pNM->home == tmdi)
    {
        LOG1N(L"Reading selection for home team: %02x", tmdi->kitSelection);
        if (g_iterHomePL != g_iterHomePL_end && !_home_forced)
        {
            // force reload
            BYTE val = tmdi->kitSelection;
            tmdi->kitSelection = (~val & 0x07) | (val & 0xf8);
            _home_forced = true;
            LOG(L"Kit reload forced for home team");
        }
    }
    else if (pNM && pNM->away == tmdi)
    {
        LOG1N(L"Reading selection for away team: %02x", tmdi->kitSelection);
        if (g_iterAwayPL != g_iterAwayPL_end && !_away_forced)
        {
            // force reload
            BYTE val = tmdi->kitSelection;
            tmdi->kitSelection = (~val & 0x07) | (val & 0xf8);
            _away_forced = true;
            LOG(L"Kit reload forced for away team");
        }
    }
}

void kservMenuEvent(int delta, DWORD menuMode, DWORD ind, 
        DWORD inGameInd, DWORD cupModeInd)
{
    //LOG2N(L"kservMenuEvent: menuMode=%02x, delta=%d", menuMode, delta);

    if (menuMode == 0x1e && delta == -1 && inGameInd==0)
    {
        DWORD menuMode2 = *(DWORD*)(data[MENU_MODE_IDX]+8);
        if (menuMode2 == 0x24)// || menuMode2 == 0x0a)
        {
            TRACE3N(L"menuMode=%02x, menuMode2=%08x, delta=%d",
                    menuMode, menuMode2, delta);
            ResetIterators();
        }
        //__asm { int 3 }
    }
    else if (menuMode == 0x0a && delta == 1 && inGameInd==0)
    {
        TRACE3N(L"menuMode=%02x, delta=%d, inGameInd=%d",
                menuMode, delta, inGameInd);
        ResetIterators();
        //__asm { int 3 }
    }
    else if (menuMode == 0x0a && delta == -1 && inGameInd==0)
    {
        DWORD menuMode2 = *(DWORD*)(data[MENU_MODE_IDX]+8);
        if (menuMode2 == 0x04)
        {
            TRACE3N(L"menuMode=%02x, menuMode2=%08x, delta=%d",
                    menuMode, menuMode2, delta);
            ResetIterators();
        }
        //__asm { int 3 }
    }
}

void kservPtrCheckCallPoint()
{
    __asm {
        pushfd 
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        add edi,8
        push edi // param
        call kservPtrCheck
        add esp,4     // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        mov ecx, dword ptr ds:[edi+8] // execute replaced code
        test ecx,ecx                  // ...
        retn
    }
}

KEXPORT void kservPtrCheck(DWORD** pptr)
{
    if (pptr && *pptr!=0)
    {
        *pptr = 0;
        if (k_kserv.debug)
            LOG(L"pointer fixed.");
    }
}

