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

extern KMOD k_kload;
extern PESINFO g_pesinfo;
extern HINSTANCE hInst;
IDirect3DDevice9* g_device = NULL;
vector<void*> g_callChains[hk_LAST];
ID3DXFont* myFonts[4][100];
bool g_bGotFormat = false;

PFNDIRECT3DCREATE9PROC g_orgDirect3DCreate9 = NULL;
BYTE g_codeDirect3DCreate9[5] = {0,0,0,0,0};
PFNCREATEDEVICEPROC g_orgCreateDevice = NULL;
PFNPRESENTPROC g_orgPresent = NULL;
PFNRESETPROC g_orgReset = NULL;


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
	//memcpy(data, dataArray[g_pesinfo.gameVersion], sizeof(data));
	
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
	
	//return new MyDirect3D9(result);

	hookOthers();
	
	return result;
}

void hookOthers() {
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

	KDrawTextAbsolute(L"TestTest", 0, 0);

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