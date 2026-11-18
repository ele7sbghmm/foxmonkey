#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <vector>

#include "server.h"
#include "draw.h"

#include <pddi/dx8/display.hpp>
#include <p3d/utility.hpp>

void SetCpuSets(HANDLE hProcess, size_t maxCoresToUse) {
    BOOL res = FALSE;
    ULONG bufferSize = 0;
    res = GetSystemCpuSetInformation(nullptr, 0, &bufferSize, hProcess, 0);

    std::vector<uint8_t> buffer(bufferSize);
    res = GetSystemCpuSetInformation(
        reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(buffer.data()),
        bufferSize, &bufferSize, hProcess, 0);

    std::vector<ULONG> ids{};

    uint8_t* ptr = buffer.data();
    uint8_t* end = ptr + buffer.size();
    while (ptr < end) {
        auto* info = reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(ptr);
        if (info->Type == CpuSetInformation)
            if (ids.size() < maxCoresToUse)
                ids.push_back(info->CpuSet.Id);
        ptr += info->Size;
    }
    if (!ids.empty())
        res = SetProcessDefaultCpuSets(hProcess, ids.data(), (ULONG)ids.size());
}

void Init(HWND hWnd) {
    IDirect3DDevice8* dev = static_cast<d3dDisplay*>(p3d::display)->GetD3DDevice();

    Draw::Init(dev);

    // ImGui::CreateContext();
    // ImGui_ImplWin32_Init(hWnd);
    // ImGui_ImplDX8_Init(device);

    HANDLE listenerThread = CreateThread(NULL, 0, RunListener, (LPVOID)(uintptr_t)NULL, 0, NULL);
    CloseHandle(listenerThread);

    HANDLE hProcess = GetCurrentProcess();
    //SetProcessAffinityMask(hProcess, 1);
    SetCpuSets(hProcess, 1);
}
