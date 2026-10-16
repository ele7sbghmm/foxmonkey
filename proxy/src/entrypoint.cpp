#include "hooks.h"

#define WIN32_MEAN_AND_LEAN
#include <windows.h>

#include <d3d8.h>
#include <MinHook.h>
#pragma comment(lib, "libMinHook.x86.lib")

typedef HRESULT(__stdcall*CreateDevice_t)(IDirect3D8*,UINT,D3DDEVTYPE,HWND,DWORD,D3DPRESENT_PARAMETERS*,IDirect3DDevice8**);
static CreateDevice_t oCreateDevice = nullptr;
HRESULT __stdcall hCreateDevice(IDirect3D8* d3d, UINT Adapter, D3DDEVTYPE DeviceType,
        HWND hWnd, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters,
        IDirect3DDevice8** ppReturnedDeviceInterface) {
    HRESULT out = oCreateDevice(d3d, Adapter, DeviceType, hWnd, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

    IDirect3DDevice8* device = *ppReturnedDeviceInterface;
    return out;
}

typedef IDirect3D8*(__stdcall*Direct3DCreate8_t)(UINT);
static Direct3DCreate8_t oDirect3DCreate8 = nullptr;
static HMODULE hmodule = nullptr;
extern "C" IDirect3D8* __stdcall Direct3DCreate8(UINT SDKVersion) {
    if (!oDirect3DCreate8) {
        char buf[MAX_PATH];
        UINT len = GetSystemDirectoryA(buf, MAX_PATH);
        if (!len) return nullptr;
        buf[MAX_PATH-1] = '\0';
        strcat_s(buf, sizeof(buf), "\\d3d8.dll");

        hmodule = LoadLibraryA(buf);
        if (!hmodule) return nullptr;

        oDirect3DCreate8 = reinterpret_cast<Direct3DCreate8_t>
            (GetProcAddress(hmodule, "Direct3DCreate8"));
        if (!oDirect3DCreate8) return nullptr;
    }

    MH_Initialize();
    hooks::install();

    IDirect3D8* d3d = oDirect3DCreate8(SDKVersion);
    void** vftable = *(void***)d3d;

    DWORD old;
    VirtualProtect(&vftable[15], 4, PAGE_EXECUTE_READWRITE, &old);
    oCreateDevice = (CreateDevice_t)vftable[15];
    vftable[15] = &hCreateDevice;
    VirtualProtect(&vftable[15], 4, old, &old);

    return d3d;
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }
