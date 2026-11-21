#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <vector>

#include "main.h"
#include "server.h"
#include "draw.h"

#include <pddi/dx8/display.hpp>
#include <p3d/utility.hpp>

static void SetCpuSets(HANDLE hProcess, size_t maxCoresToUse) {
    ULONG bufferSize = 0;
    GetSystemCpuSetInformation(nullptr, 0, &bufferSize, hProcess, 0);
    if (bufferSize == 0) return;

    std::vector<uint8_t> buffer(bufferSize);
    BOOL res = GetSystemCpuSetInformation(
        reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(buffer.data()),
        bufferSize, &bufferSize, hProcess, 0);
    if (!res) return;

    std::vector<ULONG> ids;
    ids.reserve(maxCoresToUse);

    uint8_t* ptr = buffer.data();
    uint8_t* end = ptr + bufferSize;
    while (ptr + sizeof(DWORD) <= end) {
        auto* info = reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(ptr);
        if (info->Size == 0) break;
        if (ptr + info->Size > end) break;

        if (info->Type == CpuSetInformation && ids.size() < maxCoresToUse) {
            ids.push_back(info->CpuSet.Id);
        }
        ptr += info->Size;
    }
    if (!ids.empty()) {
        SetProcessDefaultCpuSets(hProcess, ids.data(), static_cast<ULONG>(ids.size()));
    }
}

void Init(HWND hWnd) {
    (void)hWnd;
    static bool s_initialized = false;
    if (s_initialized) return;
    s_initialized = true;

    IDirect3DDevice8* dev = p3d::display ? static_cast<d3dDisplay*>(p3d::display)->GetD3DDevice() : nullptr;
    Draw::Init(dev);

    HANDLE listenerThread = CreateThread(NULL, 0, RunListener, nullptr, 0, NULL);
    if (listenerThread) CloseHandle(listenerThread);

    HANDLE hProcess = GetCurrentProcess();
    SetCpuSets(hProcess, 1);
}
