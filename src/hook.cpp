// hook.cpp
#define UNICODE
#define THISMOD &k_kload

#include <windows.h>
#include <stdio.h>
#include "shared.h"
#include "log.h"
#include "manage.h"
#include "hook.h"
#include "kload.h"
#include "hook_addr.h"
#define lang(s) getTransl("kload",s)

#include <vector>
#include <algorithm>
#include <map>
#include <hash_map>

extern KMOD k_kload;
extern PESINFO g_pesinfo;
extern HINSTANCE hInst;
IDirect3DDevice9* g_device = NULL;
vector<void*> g_callChains[hk_LAST];
ID3DXFont* myFonts[4][100];
bool g_bGotFormat = false;


map<DWORD*, DWORD> g_replacedHeaders;
map<DWORD*, DWORD>::iterator g_replacedHeadersIt = NULL;
hash_map<DWORD, IDirect3DTexture9*> g_replacedMap;
hash_map<DWORD, IDirect3DTexture9*>::iterator g_replacedMapIt;


PFNDIRECT3DCREATE9PROC g_orgDirect3DCreate9 = NULL;
BYTE g_codeDirect3DCreate9[5] = {0,0,0,0,0};
PFNCREATEDEVICEPROC g_orgCreateDevice = NULL;
PFNPRESENTPROC g_orgPresent = NULL;
PFNRESETPROC g_orgReset = NULL;

ALLVOID g_orgBeginRender1 = NULL;
ALLVOID g_orgBeginRender2 = NULL;


void hookDirect3DCreate9()
{
	BYTE g_jmp[5] = {0,0,0,0,0};
	// Put a JMP-hook on Direct3DCreate8
	TRACE(L"JMP-hooking Direct3DCreate9...");
	
	char d3dDLL[5];
	switch (g_pesinfo.gameVersion) {
	default:
		strcpy(d3dDLL, "d3d9");
		break;
	};
	
	HMODULE hD3D9 = GetModuleHandleA(d3dDLL);
	if (!hD3D9) {
	    hD3D9 = LoadLibraryA(d3dDLL);
	}
	if (hD3D9) {
		g_orgDirect3DCreate9 = (PFNDIRECT3DCREATE9PROC)GetProcAddress(hD3D9, "Direct3DCreate9");
		
		// unconditional JMP to relative address is 5 bytes.
		g_jmp[0] = 0xe9;
		DWORD addr = (DWORD)newDirect3DCreate9 - (DWORD)g_orgDirect3DCreate9 - 5;
		TRACE1N(L"New address for Direct3DCreate9: JMP %08x", addr);
		memcpy(g_jmp + 1, &addr, sizeof(DWORD));
		
		memcpy(g_codeDirect3DCreate9, g_orgDirect3DCreate9, 5);
		DWORD newProtection = PAGE_EXECUTE_READWRITE;
		DWORD g_savedProtection;
		if (VirtualProtect(g_orgDirect3DCreate9, 8, newProtection, &g_savedProtection))
		{
		    memcpy(g_orgDirect3DCreate9, g_jmp, 5);
		    TRACE(L"JMP-hook planted.");
		}
	}
	return;
}

KEXPORT void hookFunction(HOOKS h, void* addr)
{
	if (h >= hk_LAST) return;
	
	g_callChains[h].push_back(addr);
		
	return;
}

KEXPORT void unhookFunction(HOOKS h, void* addr)
{
	if (h >= hk_LAST) return;
		
	remove(g_callChains[h].begin(),	g_callChains[h].end(), addr);
	
	return;
}

void initAddresses()
{
	// select correct addresses
	memcpy(code, codeArray[g_pesinfo.gameVersion], sizeof(code));
	memcpy(data, dataArray[g_pesinfo.gameVersion], sizeof(data));
	memcpy(ltfpPatch, ltfpPatchArray[g_pesinfo.gameVersion], sizeof(ltfpPatch));
	
	ZeroMemory(myFonts, 4*100*sizeof(ID3DXFont*));
	
	return;
}

bool firstD3Dcall = true;
IDirect3D9* STDMETHODCALLTYPE newDirect3DCreate9(UINT sdkVersion) {
	// the first instance is freed after getting the device properties
	if (firstD3Dcall) {
		firstD3Dcall = false;
		
		BYTE* dest = (BYTE*)g_orgDirect3DCreate9;

		// put back saved code fragment
		dest[0] = g_codeDirect3DCreate9[0];
		*((DWORD*)(dest + 1)) = *((DWORD*)(g_codeDirect3DCreate9 + 1));

		// hook the address of the real Direct3DCreate9 call
		DWORD* ptr = (DWORD*)(code[C_D3DCREATE_CS] + 1);
		DWORD protection = 0, newProtection = PAGE_EXECUTE_READWRITE;
		if (VirtualProtect(ptr, 4, newProtection, &protection)) {
			ptr[0] = (DWORD)newDirect3DCreate9 - (code[C_D3DCREATE_CS] + 5);
		}
		
		// process the calls to the modules
		ALLVOID NextCall = NULL;
	
		TRACE(L"newDirect3DCreate9 called.");
		
		CALLCHAIN(hk_D3D_Create, it) {
			NextCall = (ALLVOID)*it;
			NextCall();
		}
		
		return g_orgDirect3DCreate9(sdkVersion);
	}

	// call the original function.
	IDirect3D9* result = g_orgDirect3DCreate9(sdkVersion);
	
	if (!g_device) {
		DWORD* vtable = (DWORD*)(*((DWORD*)result));
        
	  // hook CreateDevice method
	  g_orgCreateDevice = (PFNCREATEDEVICEPROC)vtable[VTAB_CREATEDEVICE];
	  DWORD protection = 0;
		DWORD newProtection = PAGE_EXECUTE_READWRITE;
		if (VirtualProtect(vtable+VTAB_CREATEDEVICE, 4, newProtection, &protection))
		{
			vtable[VTAB_CREATEDEVICE] = (DWORD)newCreateDevice;
			TRACE(L"CreateDevice hooked.");
		}
	}
	
	//hookOthers();
	
	return result;
}

void hookOthers() {

	DWORD newProtection = PAGE_EXECUTE_READWRITE;
	DWORD g_savedProtection;
	BYTE* bptr;
	DWORD* ptr;

	if (code[C_LOADTEXTUREFORPLAYER] != 0 && code[C_LOADTEXTUREFORPLAYER_CS] != 0)	{
		bptr = (BYTE*)code[C_LOADTEXTUREFORPLAYER_CS];
    if (VirtualProtect(bptr, 5 + LTFPLEN, newProtection, &g_savedProtection)) {
        bptr[0] = 0xe8;
        DWORD* ptr = (DWORD*)(code[C_LOADTEXTUREFORPLAYER_CS] + 1);
        ptr[0] = (DWORD)hookedLoadTextureForPlayer - (DWORD)(code[C_LOADTEXTUREFORPLAYER_CS] + 5);
        
        memcpy(bptr + 5, ltfpPatch, LTFPLEN);
    }
	}
	
	MasterHookFunction(code[C_ENDRENDERPLAYERS_CS], 0, hookedEndRenderPlayers);
	
	return;
}

HRESULT STDMETHODCALLTYPE newCreateDevice(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface)
{
	TRACE(L"newCreateDevice called.");
	
	HRESULT result = g_orgCreateDevice(self, Adapter, DeviceType, hFocusWindow,
            BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
            
	g_device = *ppReturnedDeviceInterface;
	g_device->AddRef();

	if (g_device) {
		DWORD* vtable = (DWORD*)(*((DWORD*)g_device));
		// the first vtable is overwritten by the second one on resets
		DWORD* vtable2 = *(DWORD**)(vtable - 1);
		DWORD protection = 0, protection2 = 0;
		DWORD newProtection = PAGE_EXECUTE_READWRITE;

		// hook Present method
    if (!g_orgPresent) {
			g_orgPresent = (PFNPRESENTPROC)vtable[VTAB_PRESENT];
			if (VirtualProtect(vtable+VTAB_PRESENT, 4, newProtection, &protection) &&
					VirtualProtect(vtable2+VTAB_PRESENT, 4, newProtection, &protection))
			{
				vtable[VTAB_PRESENT] = vtable2[VTAB_PRESENT] = (DWORD)newPresent;
				TRACE(L"Present hooked.");
			}
    }
    
    // hook Reset method
    if (!g_orgReset) {
			g_orgReset = (PFNRESETPROC)vtable[VTAB_RESET];
			if (VirtualProtect(vtable+VTAB_RESET, 4, newProtection, &protection) &&
					VirtualProtect(vtable2+VTAB_RESET, 4, newProtection, &protection))
			{
				vtable[VTAB_RESET] = vtable2[VTAB_RESET] = (DWORD)newReset;
				TRACE(L"Reset hooked.");
			}
    }
	}
	
	CALLCHAIN(hk_D3D_CreateDevice, it) {
		PFNCREATEDEVICEPROC NextCall = (PFNCREATEDEVICEPROC)*it;
		NextCall(self, Adapter, DeviceType, hFocusWindow,
           	BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
	}

	return result;
}

KEXPORT IDirect3DDevice9* getActiveDevice()
{
	return g_device;
}

KEXPORT void KDrawTextAbsolute(wchar_t* str, UINT x, UINT y, D3DCOLOR color, float fontSize, BYTE attr, DWORD format) {
	RECT rect = {x, y, 0xffff, 0xffff};
	
	if (!myFonts[attr%4][BYTE(2*fontSize)])
		D3DXCreateFont(g_device, fontSize * g_pesinfo.stretchY, 0, (attr & KDT_BOLD)?FW_BOLD:0, 0, attr & KDT_ITALIC, DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial", &(myFonts[attr%4][BYTE(2*fontSize)]));
	
	ID3DXFont* myFont = myFonts[attr%4][BYTE(2*fontSize)];
	
	bool needsEndScene = SUCCEEDED(g_device->BeginScene());
	
	myFont->DrawText(NULL, str, -1, &rect, format, color);
	
	if (needsEndScene)
		g_device->EndScene();
	
	return;
}

KEXPORT void KDrawText(wchar_t* str, UINT x, UINT y, D3DCOLOR color, float fontSize, BYTE attr, DWORD format) {
	KDrawTextAbsolute(str, x*g_pesinfo.stretchX, y*g_pesinfo.stretchY, color, fontSize, attr, format);
	return;
}

int abc = 0;
HRESULT STDMETHODCALLTYPE newPresent(IDirect3DDevice9* self, CONST RECT* src, CONST RECT* dest,
	HWND hWnd, LPVOID unused)
{
	
	if (!g_bGotFormat) {
		kloadGetBackBufferInfo(self);
	}

	CALLCHAIN(hk_D3D_Present, it) {
		PFNPRESENTPROC NextCall=(PFNPRESENTPROC)*it;
		NextCall(self, src, dest, hWnd, unused);
	}

	//KDrawTextAbsolute(L"TestTest", 0, 0);

	// CALL ORIGINAL FUNCTION ///////////////////
	HRESULT res = g_orgPresent(self, src, dest, hWnd, unused);

	return res;
}

HRESULT STDMETHODCALLTYPE newReset(IDirect3DDevice9* self, LPVOID params)
{
	TRACE(L"newReset: cleaning-up.");
	
	g_bGotFormat = false;
	
	for (int i = 0; i<400; i++) {
		if (myFonts[i/100][i%100]) {
			myFonts[i/100][i%100]->OnLostDevice();
		}
	}
	
	CALLCHAIN(hk_D3D_Reset, it) {
		PFNRESETPROC NextCall = (PFNRESETPROC)*it;
		NextCall(self, params);
	}
	
	HRESULT res = g_orgReset(self, params);
	TRACE(L"newReset: Reset() is done. About to return.");
	
	for (i = 0; i<400; i++) {
		if (myFonts[i/100][i%100]) {
			myFonts[i/100][i%100]->OnResetDevice();
		}
	}
	
	return res;
}

void kloadGetBackBufferInfo(IDirect3DDevice9* d3dDevice)
{
	TRACE(L"kloadGetBackBufferInfo: called.");

	IDirect3DSurface9* backBuffer;
	// get the 0th backbuffer.
	if (SUCCEEDED(d3dDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
	{
		D3DSURFACE_DESC desc;
		backBuffer->GetDesc(&desc);
		
		g_pesinfo.bbWidth = desc.Width;
		g_pesinfo.bbHeight = desc.Height;
		g_pesinfo.stretchX = g_pesinfo.bbWidth/1024.0;
		g_pesinfo.stretchY = g_pesinfo.bbHeight/768.0;

		TRACE(L"kloadGetBackBufferInfo: got new back buffer format and info.");
		g_bGotFormat = true;
		
		// release backbuffer
		backBuffer->Release();
	}
	
	return;
}

void prepareRenderPlayers() {
	TexPlayerInfo tpi;
	WORD orgTexIds[50];
	
	DWORD* startVal = *(DWORD**)(data[PLAYERDATA]);
	DWORD* nextVal = (DWORD*)*startVal;
	
	while (nextVal != startVal) {
		ZeroMemory(&tpi, sizeof(TexPlayerInfo));
		
		DWORD temp = *(nextVal + 2);
		nextVal = (DWORD*)*nextVal;
		
		if (!temp) continue;
		
		DWORD temp2 = *(DWORD*)(temp + 0x64);
		if (!temp2) continue;
		DWORD* texArr = *(DWORD**)(temp2 + 0x454);
		if (!texArr) continue;
		
		bool isReferee = (*(DWORD*)temp == data[ISREFEREEADDR]);
		if (!isReferee) {
			DWORD playerName = *(DWORD*)(temp + 0x534);
			DWORD playerInfo = playerName - 0x11c;

			tpi.playerName = (char*)playerName;
		}
		
		tpi.dummy = 123;
		tpi.lod = *(BYTE*)(temp2 + 0x4c4);
		tpi.referee = isReferee?1:0;
		//

		for (int i = 0; i < 9; i++)  {
			DWORD coll = texArr[i];
			if (!coll) continue;
				
			DWORD temp3 = *(DWORD*)(coll + 0x444);
			if (!temp3) continue;
			
			DWORD ktmdl = *(DWORD*)(temp3 + 0x120);
			if (!ktmdl) continue;
			ktmdl = *(DWORD*)(ktmdl + 0xc);
			if (!ktmdl) continue;

			DWORD ktmdlEntriesStart = ktmdl + *(DWORD*)(ktmdl + 0x54);
			WORD* texTypes = (WORD*)(ktmdl + *(DWORD*)(ktmdl + 0x80) + 8);
			BYTE orgTexMaxNum = (*(DWORD*)(ktmdl + 0x60) - *(DWORD*)(ktmdl + 0x54)) / 0x50;

			for (int j = 0; j < orgTexMaxNum; j++) {
				WORD orgTexNum = *(WORD*)(ktmdlEntriesStart + 0x50 * j + 0xe);
				orgTexIds[j] = texTypes[orgTexNum * 8];
			}

			CALLCHAIN(hk_RenderPlayer, it) {
				RENDERPLAYER NextCall = (RENDERPLAYER)*it;
				NextCall(&tpi, coll, i, orgTexIds, orgTexMaxNum);
			}
		}
	}
}

KEXPORT WORD getOrgTexId(DWORD coll, DWORD num) {
	if (!coll) return 0;
		
	DWORD temp3 = *(DWORD*)(coll + 0x444);
	if (!temp3) return 0;
	
	DWORD ktmdl = *(DWORD*)(temp3 + 0x120);
	if (!ktmdl) return 0;
	ktmdl = *(DWORD*)(ktmdl + 0xc);
	if (!ktmdl) return 0;
		
	DWORD ktmdlEntriesStart = ktmdl + *(DWORD*)(ktmdl + 0x54);
	WORD* texTypes = (WORD*)(ktmdl + *(DWORD*)(ktmdl + 0x80) + 8);
	
	WORD orgTexNum = *(WORD*)(ktmdlEntriesStart + 0x50 * num + 0xe);
	
	return texTypes[orgTexNum * 8];	
	
}

KEXPORT void setTextureHeaderAddr(DWORD* p1, IDirect3DTexture9* tex)
{
	if (!p1) return;
	
	// save the value
	g_replacedHeadersIt = g_replacedHeaders.find(p1);
	// only save if this header hasn't been replaced yet
	if (g_replacedHeadersIt ==  g_replacedHeaders.end()) {
		g_replacedHeaders[p1] = *p1;
	}
	
	// create a buffer and set it as the new value
	DWORD value = (DWORD)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 0x24);
	*(DWORD*)(value + 0x20) = (DWORD)tex;
	*p1 = value;
	return;
}

KEXPORT void setNewSubTexture(DWORD coll, BYTE num, IDirect3DTexture9* tex)
{
	if (!coll) return;
		
	DWORD temp1 = *(DWORD*)(coll + 0x444);
	if (!temp1) return;
		
	DWORD loadedNum = *(DWORD*)(temp1 + 0x15c);

	g_replacedMap[temp1 ^ ((num & 0xff) * 0x1010101)] = tex;
		
	if (num > loadedNum - 1) return;
	
	DWORD* subtextures = *(DWORD**)(temp1 + 0x164);
	if (!subtextures) return;
	
	setTextureHeaderAddr(subtextures + num, tex);
	
	return;
}

KEXPORT IDirect3DTexture9* getSubTexture(DWORD coll, DWORD num)
{
	if (!coll) return NULL;
		
	DWORD temp1 = *(DWORD*)(coll + 0x444);
	if (!temp1) return NULL;
		
	DWORD loadedNum = *(DWORD*)(temp1 + 0x15c);
	if (num > loadedNum - 1) return NULL;
	
	DWORD** subtextures = *(DWORD***)(temp1 + 0x164);
	if (!subtextures) return NULL;
		
	DWORD* temp2 = *(subtextures + num);
	if (!temp2) return NULL;
	
	return (IDirect3DTexture9*)(*(temp2 + 8));
}
	

DWORD STDMETHODCALLTYPE hookedLoadTextureForPlayer(DWORD num, DWORD newTex)
{
	DWORD _ESI, _EBP;
	__asm mov _ESI, esi
	__asm mov _EBP, ebp

	DWORD* subtextures = *(DWORD**)(_ESI + 0x164);
	DWORD loadedNum = *(DWORD*)(_ESI + 0x15c);
	bool isNewTex = false;
	
	// free our own subtexture headers
	if (subtextures) {
		for (int i=0; i < loadedNum; i++) {
			g_replacedHeadersIt = g_replacedHeaders.find(subtextures + i);
			if (g_replacedHeadersIt != g_replacedHeaders.end()) {
				HeapFree(GetProcessHeap(), 0, (VOID*)*(g_replacedHeadersIt->first));
				*(g_replacedHeadersIt->first) = g_replacedHeadersIt->second;
				g_replacedHeaders.erase(subtextures + i);
			}
		}
	}
	
	DWORD newTex1 = newTex;
	
	g_replacedMapIt = g_replacedMap.find(_ESI ^ ((num & 0xff) * 0x1010101));
	if (g_replacedMapIt != g_replacedMap.end()) {
		// create a buffer and set it as the new value
		newTex1 = (DWORD)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 0x24);
		*(DWORD*)(newTex1 + 0x20) = (DWORD)g_replacedMapIt->second;
		isNewTex = true;
	}
		
	// call the original function
	__asm mov esi, _ESI
	DWORD res = ((LOADTEXTUREFORPLAYER)code[C_LOADTEXTUREFORPLAYER])(num, newTex1);
	
	
	subtextures = *(DWORD**)(_ESI + 0x164);
	loadedNum = *(DWORD*)(_ESI + 0x15c);

	if (subtextures) {
		for (int i=0; i < loadedNum; i++) {
			if (i != num) {
				g_replacedMapIt = g_replacedMap.find(_ESI ^ ((i & 0xff) * 0x1010101));
				if (g_replacedMapIt != g_replacedMap.end()) {
					setTextureHeaderAddr(subtextures + i, g_replacedMapIt->second);
				}
			} else if (isNewTex) {
				// save the value
				g_replacedHeadersIt = g_replacedHeaders.find(subtextures + num);
				// only save if this header hasn't been replaced yet
				if (g_replacedHeadersIt ==  g_replacedHeaders.end()) {
					g_replacedHeaders[subtextures + num] = newTex;
				}
			}
		}
	}

	return newTex1;
}

DWORD hookedEndRenderPlayers() {
	DWORD _EDI;
	__asm mov _EDI, edi
	
	prepareRenderPlayers();
	
	__asm mov edi, _EDI
	DWORD res = MasterCallNext();
	
	// free all replaced buffers and reset them to the original ones
	for (g_replacedHeadersIt = g_replacedHeaders.begin(); 
			g_replacedHeadersIt != g_replacedHeaders.end();
			g_replacedHeadersIt++)
		{
			HeapFree(GetProcessHeap(), 0, (VOID*)*(g_replacedHeadersIt->first));
			*(g_replacedHeadersIt->first) = g_replacedHeadersIt->second;
		}
	g_replacedHeaders.clear();
	
	return res;
}