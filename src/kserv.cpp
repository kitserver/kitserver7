/* Kitserver module */
#define UNICODE
#define THISMOD &k_kserv

#include <windows.h>
#include <stdio.h>
#include "kload_exp.h"
#include "kserv.h"
#include "kserv_addr.h"
#include "dllinit.h"

#define lang(s) getTransl("kserv",s)

#include <map>
#include <hash_map>

typedef struct _UNPACK_INFO 
{
    BYTE* srcBuffer;
    DWORD bytesToProcess;
    DWORD bytesProcessed;
    BYTE* destBuffer;
    DWORD unknown1; // 0
    DWORD destSize;
} UNPACK_INFO;

// VARIABLES
HINSTANCE hInst = NULL;
KMOD k_kserv = {MODID, NAMELONG, NAMESHORT, DEFAULT_DEBUG};
bool allQualities = true;

// GLOBALS
DWORD g_kitsAfsIds[] = {7919,7920,3193,3194,3195,3196,4217,4218,4219,4220};
hash_map<DWORD,BIN_INFO> g_kitsBinInfoById;
hash_map<DWORD,BIN_INFO> g_kitsBinInfoByOffset;
DWORD g_currentBin = 0xffffffff;

hash_map<DWORD,DWORD> g_kitNameAndNumbersSwap;

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
PACKED_BIN* LoadBinFromAFS(DWORD id);


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
	}
	
	return true;
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
        for (int i=0; i<sizeof(g_kitsAfsIds)/sizeof(DWORD); i++)
        {
            BIN_INFO binInfo;
            binInfo.id = g_kitsAfsIds[i];
            ReadItemInfoById(f, binInfo.id, &binInfo.itemInfo, 0);

            g_kitsBinInfoById.insert(pair<DWORD,BIN_INFO>(binInfo.id, binInfo));
            g_kitsBinInfoByOffset.insert(pair<DWORD,BIN_INFO>(binInfo.itemInfo.dwOffset, binInfo));
        }
        fclose(f);
    }
    else
    {
        LOG(L"ERROR: Unable to initialize AFS itemInfo structures.");
    }

    // initialize name-and-numbers swap
    g_kitNameAndNumbersSwap.insert(pair<DWORD,DWORD>(3193,3194));
    g_kitNameAndNumbersSwap.insert(pair<DWORD,DWORD>(3194,3193));
    g_kitNameAndNumbersSwap.insert(pair<DWORD,DWORD>(3195,3196));
    g_kitNameAndNumbersSwap.insert(pair<DWORD,DWORD>(3196,3195));
    g_kitNameAndNumbersSwap.insert(pair<DWORD,DWORD>(4217,4218));
    g_kitNameAndNumbersSwap.insert(pair<DWORD,DWORD>(4218,4217));
    g_kitNameAndNumbersSwap.insert(pair<DWORD,DWORD>(4219,4220));
    g_kitNameAndNumbersSwap.insert(pair<DWORD,DWORD>(4220,4219));

    // hook UnpackBin
    MasterHookFunction(code[C_UNPACK_BIN], 2, kservUnpackBin);
    // hook SetFilePointerEx
    HookSetFilePointerEx();
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

    // remember the unpack BIN address, because the call will change it.
    UNPACKED_BIN* bin = (UNPACKED_BIN*)pUnpackInfo->destBuffer;

    hash_map<DWORD,BIN_INFO>::iterator it = g_kitsBinInfoById.find(g_currentBin);
    if (it != g_kitsBinInfoById.end()) // bin found in map.
    {
        LOG1N(L"Unpacking bin %d", g_currentBin);

        // test: replace whole bins for numbers and fonts
        hash_map<DWORD,DWORD>::iterator nit = g_kitNameAndNumbersSwap.find(g_currentBin);
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
    }

    // call original
    DWORD result = MasterCallNext(pUnpackInfo, p2);

    // release swap bin memory
    if (swap_bin)
        HeapFree(GetProcessHeap(), 0, swap_bin);

    if (it != g_kitsBinInfoById.end()) // bin found in map.
    {
        LOG1N(L"num entries = %d", bin->header.numEntries);
        // replace textures
        // TODO
        // test: swap textures
        if (bin->header.numEntries == 2 && 
                bin->entryInfo[0].size == 0x40410 &&
                bin->entryInfo[1].size == 0x40410)
        {
            TEXTURE_ENTRY* tex0 = (TEXTURE_ENTRY*)((BYTE*)bin + bin->entryInfo[0].offset);
            TEXTURE_ENTRY* tex1 = (TEXTURE_ENTRY*)((BYTE*)bin + bin->entryInfo[1].offset);

            BYTE* tmp = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bin->entryInfo[0].size);
            memcpy(tmp, tex0, bin->entryInfo[0].size);
            memcpy(tex0, tex1, bin->entryInfo[1].size);
            memcpy(tex1, tmp, bin->entryInfo[1].size);
            HeapFree(GetProcessHeap(), 0, tmp);
        }
    }
    // reset bin indicator
    g_currentBin = 0xffffffff;

    return result;
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
        LOG(L"ERROR: Unable to load BIN #%d", id);
    }
    return result;
}

