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

#define lang(s) getTransl("kserv",s)

#include <map>
#include <hash_map>
#include <wchar.h>

#define SWAPBYTES(dw) \
    (dw<<24 & 0xff000000) | (dw<<8  & 0x00ff0000) | \
    (dw>>8  & 0x0000ff00) | (dw>>24 & 0x000000ff)

typedef struct _UNPACK_INFO 
{
    BYTE* srcBuffer;
    DWORD bytesToProcess;
    DWORD bytesProcessed;
    BYTE* destBuffer;
    DWORD unknown1; // 0
    DWORD destSize;
} UNPACK_INFO;

typedef struct _READ_RADAR_INFO
{
    DWORD unknown1;
    WORD teamId;
} READ_RADAR_INFO;

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

// VARIABLES
HINSTANCE hInst = NULL;
KMOD k_kserv = {MODID, NAMELONG, NAMESHORT, DEFAULT_DEBUG};
bool allQualities = true;

#define NUM_TEAMS 299
#define NUM_SLOTS 256
#define FIRST_KIT_BIN  7523
#define FIRST_FONT_BIN 2401
#define FIRST_NUMS_BIN 3425

// GLOBALS
hash_map<DWORD,BIN_INFO> g_kitsBinInfoById;
hash_map<DWORD,BIN_INFO> g_kitsBinInfoByOffset;
DWORD g_currentBin = 0xffffffff;
hash_map<WORD,WORD> g_teamIdByKitSlot; // to lookup team ID, using kit slot
hash_map<WORD,WORD> g_kitSlotByTeamId; // to lookup kit slot, using team ID
DWORD g_teamKitInfoBase = 0;
bool g_slotMapsInitialized = false;

GDB* gdb = NULL;
DWORD g_return_addr = 0;
DWORD g_param = 0;

// FUNCTIONS
void initKserv();
void kservRenderPlayer(TexPlayerInfo* tpi, DWORD coll, DWORD num, WORD* orgTexIds, BYTE orgTexMaxNum);
BOOL WINAPI kservSetFilePointerEx(
  __in       HANDLE hFile,
  __in       LARGE_INTEGER liDistanceToMove,
  __out_opt  PLARGE_INTEGER lpNewFilePointer,
  __in       DWORD dwMoveMethod
);
DWORD kservUnpackBin(UNPACK_INFO* pUnpackInfo, DWORD p2);
void HookSetFilePointerEx();
void HookCallPoint(DWORD addr, void* func, int codeShift, int numNops);
void HookAt(DWORD addr, void* func);
PACKED_BIN* LoadBinFromAFS(DWORD id);
void DumpData(void* data, size_t size);
DWORD LoadPNGTexture(BITMAPINFO** tex, const wchar_t* filename);
void ApplyAlphaChunk(RGBQUAD* palette, BYTE* memblk, DWORD size);
static int read_file_to_mem(const wchar_t *fn,unsigned char **ppfiledata, int *pfilesize);
void ApplyDIBTexture(TEXTURE_ENTRY* tex, BITMAPINFO* bitmap, bool adjustPalette);
void FreePNGTexture(BITMAPINFO* bitmap);
void ReplaceTexturesInBin(UNPACKED_BIN* bin, wstring files[], bool* flags, size_t n);
WORD GetTeamIdBySrc(TEAM_KIT_INFO* src);
TEAM_KIT_INFO* GetTeamKitInfoById(WORD id);
void kservAfterReadTeamKitInfo(TEAM_KIT_INFO* src, TEAM_KIT_INFO* dest);
void kservReadTeamKitInfoCallPoint1();
void kservReadTeamKitInfoCallPoint2();
void kservReadRadarInfoCallPoint();
void kservBeforeReadRadarInfo(READ_RADAR_INFO** ppReadRadarInfo);
void kservAfterReadRadarInfo(READ_RADAR_INFO** ppReadRadarInfo);
void InitSlotMap();
int GetBinType(DWORD id);
int GetKitSlot(DWORD id);
WORD FindFreeKitSlot();
void ApplyKitAttributes(map<wstring,Kit>& m, const wchar_t* kitKey, KIT_INFO& ki);

// FUNCTION POINTERS
DWORD g_orgReadRadarInfo = 0;

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
			TRACE(L"Sorry, your game version isn't supported!");
			return false;
		}

		copyAdresses();
		hookFunction(hk_D3D_Create, initKserv);
	}
	
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		TRACE(L"Shutting down this module...");
        delete gdb;
	}
	
	return true;
}

void InitSlotMap()
{
    if (!g_teamKitInfoBase)
    {
        LOG(L"ERROR: Unable to initialize slot maps: g_teamKitInfoBase = 0");
        return;
    }

    TEAM_KIT_INFO* teamKitInfo = (TEAM_KIT_INFO*)g_teamKitInfoBase;
    for (WORD i=0; i<NUM_TEAMS; i++)
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
        pushf
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        sub eax,edx
        mov g_teamKitInfoBase, eax
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
        popf
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
        pushf
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
        popf
        mov esi,dword ptr ss:[esp+0x2c0]  // execute replaced code (stack offset: extra 4 bytes)
        retn
    }
}

void kservReadRadarInfoCallPoint()
{
    __asm
    {
        pushf
        push ebp
        // save return addr
        mov ebp,[esp+6]
        mov g_return_addr,ebp
        mov g_param,eax
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push eax      // parameter
        call kservBeforeReadRadarInfo
        add esp,4     // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popf
        add esp,4
        call g_orgReadRadarInfo  // execute replaced code
        pushf
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push g_param  // parameter
        call kservAfterReadRadarInfo
        add esp,4     // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popf
        push g_return_addr
        retn
    }
}

void initKserv() {
	TRACE(L"Going to hook some functions now.");
	
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
	
	#ifndef MYDLL_RELEASE_BUILD
	hookFunction(hk_RenderPlayer, kservRenderPlayer);
	#endif
	
	TRACE(L"Hooking done.");
	unhookFunction(hk_D3D_Create, initKserv);

    // initialize kits iteminfo structure
    FILE* f = fopen("img/cv_0.img","rb");
    if (f)
    {
        //for (int i=0; i<sizeof(g_kitsAfsIds)/sizeof(DWORD); i++)
        for (int i=0; i<NUM_SLOTS; i++)
        {
            int k;
            for (k=0; k<2; k++)
            {
                BIN_INFO binInfo;
                binInfo.id = FIRST_KIT_BIN + i*2 + k;
                ReadItemInfoById(f, binInfo.id, &binInfo.itemInfo, 0);

                g_kitsBinInfoById.insert(pair<DWORD,BIN_INFO>(binInfo.id, binInfo));
                g_kitsBinInfoByOffset.insert(pair<DWORD,BIN_INFO>(binInfo.itemInfo.dwOffset, binInfo));
            }
            for (k=0; k<4; k++)
            {
                BIN_INFO binInfo;
                binInfo.id = FIRST_FONT_BIN + i*4 + k;
                ReadItemInfoById(f, binInfo.id, &binInfo.itemInfo, 0);

                g_kitsBinInfoById.insert(pair<DWORD,BIN_INFO>(binInfo.id, binInfo));
                g_kitsBinInfoByOffset.insert(pair<DWORD,BIN_INFO>(binInfo.itemInfo.dwOffset, binInfo));
            }
            for (k=0; k<4; k++)
            {
                BIN_INFO binInfo;
                binInfo.id = FIRST_NUMS_BIN + i*4 + k;
                ReadItemInfoById(f, binInfo.id, &binInfo.itemInfo, 0);

                g_kitsBinInfoById.insert(pair<DWORD,BIN_INFO>(binInfo.id, binInfo));
                g_kitsBinInfoByOffset.insert(pair<DWORD,BIN_INFO>(binInfo.itemInfo.dwOffset, binInfo));
            }
        }
        fclose(f);
        LOG(L"AFS itemInfo structures initialized.");
    }
    else
    {
        LOG(L"ERROR: Unable to initialize AFS itemInfo structures.");
    }

    // Load GDB
    gdb = new GDB(L".\\kitserver\\");

    // hook UnpackBin
    MasterHookFunction(code[C_UNPACK_BIN], 2, kservUnpackBin);
    // hook SetFilePointerEx
    HookSetFilePointerEx();
    // hook reading of team kit info
    HookCallPoint(code[C_AFTER_READTEAMKITINFO_1], kservReadTeamKitInfoCallPoint1, 6, data[NUMNOPS_1]);
    HookCallPoint(code[C_AFTER_READTEAMKITINFO_2], kservReadTeamKitInfoCallPoint2, 6, data[NUMNOPS_2]);
    // hook radar info function
    //g_orgReadRadarInfo = code[C_READ_RADAR_INFO_TARGET];
    //HookCallPoint(code[C_READ_RADAR_INFO], kservReadRadarInfoCallPoint, 6, 0);
}

void HookSetFilePointerEx()
{
	if (code[C_SETFILEPOINTEREX] != 0)
	{
	    BYTE* bptr = (BYTE*)code[C_SETFILEPOINTEREX];
	
	    DWORD protection = 0;
	    DWORD newProtection = PAGE_EXECUTE_READWRITE;
	    if (VirtualProtect(bptr, 8, newProtection, &protection)) {
	        bptr[0] = 0xe8; bptr[5] = 0x90; // NOP
	        DWORD* ptr = (DWORD*)(code[C_SETFILEPOINTEREX] + 1);
	        ptr[0] = (DWORD)kservSetFilePointerEx - (DWORD)(code[C_SETFILEPOINTEREX] + 5);
	        LOG(L"SetFilePointerEx HOOKED at code[C_SETFILEPOINTEREX]");
	    }
	}
}

void HookCallPoint(DWORD addr, void* func, int codeShift, int numNops)
{
    DWORD target = (DWORD)func + codeShift;
	if (addr && target)
	{
	    BYTE* bptr = (BYTE*)addr;
	    DWORD protection = 0;
	    DWORD newProtection = PAGE_EXECUTE_READWRITE;
	    if (VirtualProtect(bptr, 8, newProtection, &protection)) {
	        bptr[0] = 0xe8;
	        DWORD* ptr = (DWORD*)(addr + 1);
	        ptr[0] = target - (DWORD)(addr + 5);
            // padding with NOPs
            for (int i=0; i<numNops; i++) bptr[5+i] = 0x90;
	        LOG2N(L"Function (%08x) HOOKED at address (%08x)", target, addr);
	    }
	}
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

BOOL WINAPI kservSetFilePointerEx(
  __in       HANDLE hFile,
  __in       LARGE_INTEGER liDistanceToMove,
  __out_opt  PLARGE_INTEGER lpNewFilePointer,
  __in       DWORD dwMoveMethod
)
{
    DWORD offset = liDistanceToMove.LowPart;
    hash_map<DWORD,BIN_INFO>::iterator it = g_kitsBinInfoByOffset.find(offset);
    if (it != g_kitsBinInfoByOffset.end())
    {
        g_currentBin = it->second.id;
    }

    // call original
    BOOL result = SetFilePointerEx(hFile, liDistanceToMove, lpNewFilePointer, dwMoveMethod);
    return result;
}

DWORD kservUnpackBin(UNPACK_INFO* pUnpackInfo, DWORD p2)
{
    PACKED_BIN* swap_bin = NULL;
    DWORD currBin = g_currentBin;
    // reset bin indicator
    g_currentBin = 0xffffffff;

    // remember the unpack BIN address, because the call will change it.
    UNPACKED_BIN* bin = (UNPACKED_BIN*)pUnpackInfo->destBuffer;

    hash_map<DWORD,BIN_INFO>::iterator it = g_kitsBinInfoById.find(currBin);
    if (it != g_kitsBinInfoById.end()) // bin found in map.
    {
        LOG1N(L"Unpacking bin %d", currBin);

        /*
        // test: replace whole bins for numbers and fonts
        hash_map<DWORD,DWORD>::iterator nit = g_kitNameAndNumbersSwap.find(currBin);
        if (nit != g_kitNameAndNumbersSwap.end())
        {
            // NOTE: this generic technique appears to work well for replacing
            // bins at run-time: we modify the address pointer to PACKED_BIN data
            // and also the number of bytes to process. 
            //
            // There is an assumption that unpacked bin will be of the same size
            // (as it's the case with textures, for instance, but not 3d-models).
            // If a larger buffer for unpacked bin is needed, then we might be able
            // to achieve that by modifying pUnpackInfo->destBuffer and pUnpackInfo->destSize.
            // But this needs to be verified.
            
            swap_bin = LoadBinFromAFS(nit->second); // bin can be loaded from disk too
            pUnpackInfo->srcBuffer = swap_bin->data;
            pUnpackInfo->bytesToProcess = swap_bin->header.sizePacked;
        }
        */
    }

    // call original
    DWORD result = MasterCallNext(pUnpackInfo, p2);

    // release swap bin memory
    if (swap_bin)
        HeapFree(GetProcessHeap(), 0, swap_bin);

    if (it != g_kitsBinInfoById.end()) // bin found in map.
    {
        LOG1N(L"num entries = %d", bin->header.numEntries);
        //DumpData((BYTE*)bin, pUnpackInfo->destSize);

        // determine type of bin
        int type = GetBinType(currBin);

        // determine team ID by kit slot
        WORD kitSlot = GetKitSlot(currBin);
        hash_map<WORD,WORD>::iterator ksit = g_teamIdByKitSlot.find(kitSlot);
        if (ksit != g_teamIdByKitSlot.end())
        {
            WORD teamId = ksit->second;
            hash_map<WORD,KitCollection>::iterator wkit = gdb->uni.find(teamId);
            if (wkit != gdb->uni.end())
            {
                // found this team in GDB.
                // replace textures
                if (bin->header.numEntries == 2 && 
                        bin->entryInfo[0].size == 0x40410 &&
                        bin->entryInfo[1].size == 0x40410)
                {
                    wchar_t filename[BUFLEN] = {L'\0'};
                    switch (type)
                    {
                        case BIN_KIT_GK:
                            {
                                wstring files[] = {
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\ga\\kit.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\gb\\kit.png",
                                };
                                bool flags[] = {true,true};
                                ReplaceTexturesInBin(bin, files, flags, 2);
                            }
                            break;
                        case BIN_KIT_PL:
                            {
                                wstring files[] = {
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\pa\\kit.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\pb\\kit.png",
                                };
                                bool flags[] = {true,true};
                                ReplaceTexturesInBin(bin, files, flags, 2);
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
                                            + L"\\ga\\font.png",
                                };
                                bool flags[] = {false,false};
                                ReplaceTexturesInBin(bin, files, flags, 1);
                            }
                            break;
                        case BIN_FONT_GB:
                            {
                                wstring files[] = {
                                        gdb->dir + L"GDB\\uni\\"
                                            + wkit->second.foldername 
                                            + L"\\gb\\font.png",
                                };
                                bool flags[] = {false,false};
                                ReplaceTexturesInBin(bin, files, flags, 1);
                            }
                            break;
                        case BIN_FONT_PA:
                            {
                                wstring files[] = {
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\pa\\font.png",
                                };
                                bool flags[] = {false,false};
                                ReplaceTexturesInBin(bin, files, flags, 1);
                            }
                            break;
                        case BIN_FONT_PB:
                            {
                                wstring files[] = {
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\pb\\font.png",
                                };
                                bool flags[] = {false,false};
                                ReplaceTexturesInBin(bin, files, flags, 1);
                            }
                            break;
                        case BIN_NUMS_GA:
                            {
                                wstring files[] = {
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\ga\\numbers-back.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\ga\\numbers-front.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\ga\\numbers-shorts.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\ga\\numbers-shorts.png",
                                };
                                bool flags[] = {false,false,false,false};
                                ReplaceTexturesInBin(bin, files, flags, 4);
                            }
                            break;
                        case BIN_NUMS_GB:
                            {
                                wstring files[] = {
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\gb\\numbers-back.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\gb\\numbers-front.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\gb\\numbers-shorts.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\gb\\numbers-shorts.png",
                                };
                                bool flags[] = {false,false,false,false};
                                ReplaceTexturesInBin(bin, files, flags, 4);
                            }
                            break;
                        case BIN_NUMS_PA:
                            {
                                wstring files[] = {
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\pa\\numbers-back.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\pa\\numbers-front.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\pa\\numbers-shorts.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\pa\\numbers-shorts.png",
                                };
                                bool flags[] = {false,false,false,false};
                                ReplaceTexturesInBin(bin, files, flags, 4);
                            }
                            break;
                        case BIN_NUMS_PB:
                            {
                                wstring files[] = {
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\pb\\numbers-back.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\pb\\numbers-front.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\pb\\numbers-shorts.png",
                                        gdb->dir + L"GDB\\uni\\" 
                                            + wkit->second.foldername 
                                            + L"\\pb\\numbers-shorts.png",
                                };
                                bool flags[] = {false,false,false,false};
                                ReplaceTexturesInBin(bin, files, flags, 4);
                            }
                            break;
                    }
                }
            } // end if wkit
        } // end if ksit
    }

    return result;
}

void ReplaceTexturesInBin(UNPACKED_BIN* bin, wstring files[], bool* flags, size_t n)
{
    for (int i=0; i<n; i++)
    {
        TEXTURE_ENTRY* tex = (TEXTURE_ENTRY*)((BYTE*)bin + bin->entryInfo[i].offset);
        BITMAPINFO* bmp = NULL;
        DWORD texSize = LoadPNGTexture(&bmp, files[i].c_str());
        if (texSize)
        {
            ApplyDIBTexture(tex, bmp, flags[i]);
            FreePNGTexture(bmp);
        }
    }
}

PACKED_BIN* LoadBinFromAFS(DWORD id)
{
    PACKED_BIN* result = NULL;
    FILE* f = fopen("img/cv_0.img","rb");
    if (f)
    {
        AFSITEMINFO itemInfo;
        ReadItemInfoById(f, id, &itemInfo, 0);

        // allocate memory
        result = (PACKED_BIN*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, itemInfo.dwSize);

        // read into memory
        fseek(f, itemInfo.dwOffset, SEEK_SET);
        fread(result, itemInfo.dwSize, 1, f);
        fclose(f);
    }
    else 
    {
        LOG1N(L"ERROR: Unable to load BIN #%d", id);
    }
    return result;
}

void DumpData(void* data, size_t size)
{
    static count = 0;
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
        LOG1N(L"ERROR: unable to dump file (count=%d)",count);
    }
    count++;
}

// Load texture from PNG file. Returns the size of loaded texture
DWORD LoadPNGTexture(BITMAPINFO** tex, const wchar_t* filename)
{
	TRACE1S(L"LoadPNGTexture: loading %s", (wchar_t*)filename);
    DWORD size = 0;

    PNGDIB *pngdib;
    LPBITMAPINFOHEADER* ppDIB = (LPBITMAPINFOHEADER*)tex;

    pngdib = pngdib_p2d_init();
	//TRACE(L"LoadPNGTexture: structure initialized");

    BYTE* memblk;
    int memblksize;
    if(read_file_to_mem(filename,&memblk, &memblksize)) {
        TRACE(L"LoadPNGTexture: unable to read PNG file");
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
		TRACE(L"LoadPNGTexture: ERROR - unable to load PNG image.");
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
        }
        // move on to next chunk
        offset += sizeof(chunk->dwSize) + sizeof(chunk->dwName) + 
            SWAPBYTES(chunk->dwSize) + sizeof(DWORD); // last one is CRC
    }

    // initialize alpha to all-opaque, if haven't gotten it
    if (!got_alpha) {
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
void ApplyDIBTexture(TEXTURE_ENTRY* tex, BITMAPINFO* bitmap, bool adjustPalette)
{
    TRACE(L"Applying DIB texture");

	BYTE* srcTex = (BYTE*)bitmap;
	BITMAPINFOHEADER* bih = &bitmap->bmiHeader;
	DWORD palOff = bih->biSize;
    DWORD numColors = 1 << bih->biBitCount;
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
        if (adjustPalette)
            // adjust palette to KONAMI range (0:0xff --> 0x80:0xff)
            tex->palette[i].a = 0x80 + (tex->palette[i].a >> 1);
	}
	TRACE(L"Palette copied.");

	int k, m, j, w;
	int height, width;
	int imageWidth;

    width = imageWidth = tex->header.width; height = tex->header.height;

	// copy pixel data
    if (bih->biBitCount == 8)
    {
        for (k=0, m=bih->biHeight-1; k<height, m>=bih->biHeight - height; k++, m--)
        {
            memcpy(tex->data + k*width, srcTex + bitsOff + m*imageWidth, width);
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
    return ((DWORD)src - (DWORD)g_teamKitInfoBase) >> 9;
}

TEAM_KIT_INFO* GetTeamKitInfoById(WORD id)
{
    return (TEAM_KIT_INFO*)(g_teamKitInfoBase + (id<<9));
}

void kservAfterReadTeamKitInfo(TEAM_KIT_INFO* dest, TEAM_KIT_INFO* src)
{
    //LOG3N(L"kservAfterReadTeamKitInfo: team = %d, dest = %08x, src = %08x", 
    //        GetTeamIdBySrc(src), (DWORD)dest, (DWORD)src);

    if (!g_slotMapsInitialized)
    {
        InitSlotMap();
        g_slotMapsInitialized = true;
    }

    WORD teamId = GetTeamIdBySrc(src);
    hash_map<WORD,KitCollection>::iterator it = gdb->uni.find(teamId);
    if (it != gdb->uni.end())
    {
        // re-link, if necessary
        hash_map<WORD,WORD>::iterator tid = g_kitSlotByTeamId.find(teamId);
        if (tid == g_kitSlotByTeamId.end())
        {
            LOG1N(L"Re-linking team %d...", teamId);
            // this is a team in GDB, but without a kit slot in AFS
            // We need to temporarily re-link it to an available slot.
            WORD kitSlot = FindFreeKitSlot();
            if (kitSlot != 0xffff)
            {
                LOG2N(L"Team %d now uses slot 0x%04x", teamId, kitSlot);
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
        ApplyKitAttributes(it->second.goalkeepers,L"ga",dest->ga);
        ApplyKitAttributes(it->second.goalkeepers,L"gb",dest->gb);
        ApplyKitAttributes(it->second.players,L"pa",dest->pa);
        ApplyKitAttributes(it->second.players,L"pb",dest->pb);
    }
}

void ApplyKitAttributes(map<wstring,Kit>& m, const wchar_t* kitKey, KIT_INFO& ki)
{
    map<wstring,Kit>::iterator kiter = m.find(kitKey);
    if (kiter != m.end())
    {
        if (kiter->second.attDefined & MODEL)
            ki.model = kiter->second.model;
        if (kiter->second.attDefined & COLLAR)
            ki.collar = kiter->second.collar;
        if (kiter->second.attDefined & SHIRT_NUMBER_LOCATION)
            ki.frontNumberPosition = kiter->second.shirtNumberLocation;
        if (kiter->second.attDefined & SHORTS_NUMBER_LOCATION)
            ki.shortsNumberPosition = kiter->second.shortsNumberLocation;
        if (kiter->second.attDefined & NAME_LOCATION)
            ki.fontEnabled = kiter->second.nameLocation;
        if (kiter->second.attDefined & NAME_SHAPE)
            ki.fontStyle = kiter->second.nameShape;
        //if (kiter->second.attDefined & LOGO_LOCATION)
        //    ki.logoLocation = kiter->second.logoLocation;
        // radar: the game seems to read it elsewhere
        // shorts main color
        if (kiter->second.attDefined & SHORTS_MAIN_COLOR)
        {
            //TODO: convert RGBAColor to KCOLOR
        }
    }
}

void kservBeforeReadRadarInfo(READ_RADAR_INFO** ppReadRadarInfo)
{
    WORD teamId[2];
    teamId[0] = ppReadRadarInfo[0]->teamId;
    teamId[1] = ppReadRadarInfo[1]->teamId;

    for (int i=0; i<2; i++)
    {
        TEAM_KIT_INFO* tki = GetTeamKitInfoById(teamId[i]);
        LOG1N(L"Before reading radar::tki = %08x", (DWORD)tki);
        switch (teamId[i])
        {
            case 21: // set radar-color for Russia
                tki->pa.radarColor = 0x8c72; //red
                tki->pb.radarColor = 0xffff; //white
                break;
        }
    }
    LOG2N(L"Before reading radar info for teams: %d, %d", teamId[0], teamId[1]);
}

void kservAfterReadRadarInfo(READ_RADAR_INFO** ppReadRadarInfo)
{
    WORD teamId[2];
    teamId[0] = ppReadRadarInfo[0]->teamId;
    teamId[1] = ppReadRadarInfo[1]->teamId;

    for (int i=0; i<2; i++)
    {
        TEAM_KIT_INFO* tki = GetTeamKitInfoById(teamId[i]);
        LOG1N(L"After reading radar::tki = %08x", (DWORD)tki);
        switch (teamId[i])
        {
            case 21: // set radar-color for Russia
                tki->pa.radarColor = 0xffff; //white
                tki->pb.radarColor = 0x8c72; //red
                break;
        }
    }
    LOG2N(L"After reading radar info for teams: %d, %d", teamId[0], teamId[1]);
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
    return -1;
}

WORD FindFreeKitSlot()
{
    TEAM_KIT_INFO* teamKitInfo = (TEAM_KIT_INFO*)g_teamKitInfoBase;
    for (WORD i=0; i<NUM_SLOTS; i++)
    {
        hash_map<WORD,WORD>::iterator tid = g_teamIdByKitSlot.find(i);
        if (tid == g_teamIdByKitSlot.end())
            return i;
    }
    return -1;
}


