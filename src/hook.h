// hook.h
#ifndef _HOOK_
#define _HOOK_

#include "d3d9types.h"
#include "d3d9.h"
#include "d3dx9tex.h"



enum HOOKS {
	hk_D3D_Create,
	hk_D3D_CreateDevice,
	hk_D3D_Present,
	hk_D3D_Reset,
	
	hk_LAST,
};

#define KDT_BOLD 0x01
#define KDT_ITALIC 0x02
KEXPORT void KDrawTextAbsolute(wchar_t* str, UINT x, UINT y, D3DCOLOR color = 0xff000000, float fontSize = 16.0f, BYTE attr = KDT_BOLD, DWORD format = DT_LEFT);
KEXPORT void KDrawText(wchar_t* str, UINT x, UINT y, D3DCOLOR color = 0xff000000, float fontSize = 16.0f, BYTE attr = KDT_BOLD, DWORD format = DT_LEFT);

KEXPORT void hookFunction(HOOKS h, void* addr);
KEXPORT void unhookFunction(HOOKS h, void* addr);
void initAddresses();

void hookDirect3DCreate9();
void hookOthers();


// IDirect3D9
#define VTAB_CREATEDEVICE 16
// IDirect3DDevice9
#define VTAB_RESET 16
#define VTAB_PRESENT 17

typedef void   (*ALLVOID)();
typedef DWORD   (*RETDWORD)();
typedef IDirect3D9* (STDMETHODCALLTYPE *PFNDIRECT3DCREATE9PROC)(UINT sdkVersion);
typedef HRESULT (STDMETHODCALLTYPE *PFNCREATEDEVICEPROC)(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface);
typedef HRESULT (STDMETHODCALLTYPE *PFNPRESENTPROC)(IDirect3DDevice9* self, 
		CONST RECT* src, CONST RECT* dest, HWND hWnd, LPVOID unused);
typedef HRESULT (STDMETHODCALLTYPE *PFNRESETPROC)(IDirect3DDevice9* self, LPVOID);

IDirect3D9* STDMETHODCALLTYPE newDirect3DCreate9(UINT sdkVersion);
HRESULT STDMETHODCALLTYPE newCreateDevice(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface);
KEXPORT IDirect3DDevice9* getActiveDevice();
HRESULT STDMETHODCALLTYPE newPresent(IDirect3DDevice9* self, CONST RECT* src, CONST RECT* dest,
	HWND hWnd, LPVOID unused);
HRESULT STDMETHODCALLTYPE newReset(IDirect3DDevice9* self, LPVOID params);
void kloadGetBackBufferInfo(IDirect3DDevice9* d3dDevice);

#endif
