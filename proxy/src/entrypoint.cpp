#include "hooks.h"

#define WIN32_MEAN_AND_LEAN
#include <windows.h>

#include "MinHook.h"
#pragma comment(lib, "libMinHook.x86.lib")

struct IDirect3D8;

typedef IDirect3D8*(__stdcall*Create_t)(UINT);
static Create_t original = nullptr;
static HMODULE hmodule = nullptr;

extern "C" IDirect3D8* __stdcall Direct3DCreate8(UINT SDKVersion) {
    if (!original) {
        char buf[MAX_PATH];
        UINT len = GetSystemDirectoryA(buf, MAX_PATH);
        if (!len) return nullptr;
        buf[MAX_PATH-1] = '\0';
        strcat_s(buf, sizeof(buf), "\\d3d8.dll");

        hmodule = LoadLibraryA(buf);
        if (!hmodule) return nullptr;

        original = reinterpret_cast<Create_t>
            (GetProcAddress(hmodule, "Direct3DCreate8"));
        if (!original) return nullptr;
    }

    MH_Initialize();
    hooks::install();

    return original(SDKVersion);
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }
