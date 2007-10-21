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

// VARIABLES
HINSTANCE hInst = NULL;
KMOD k_kserv = {MODID, NAMELONG, NAMESHORT, DEFAULT_DEBUG};
bool allQualities = true;


// FUNCTIONS
void initKserv();

void kservRenderPlayer(TexPlayerInfo* tpi, DWORD coll, DWORD num);


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
			}
		}
	}
	
	hookFunction(hk_RenderPlayer, kservRenderPlayer);
	
	TRACE(L"Hooking done.");
	unhookFunction(hk_D3D_Create, initKserv);
	
	return;
}


void DumpTexture(IDirect3DTexture9* const ptexture, DWORD a) 
{
    wchar_t buf[BUFLEN];
    swprintf(buf, L"kitserver\\tex-%08x-%08x.bmp", a, (DWORD)ptexture);
    D3DXSaveTextureToFile(buf, D3DXIFF_BMP, ptexture, NULL);
}

void kservRenderPlayer(TexPlayerInfo* tpi, DWORD coll, DWORD num)
{
	if (num != 0) return;
	
	for (int j=0; j<20; j++)
		setNewSubTexture(coll, j, NULL);

	return;
}