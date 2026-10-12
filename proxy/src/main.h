#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx8.h"

#include "shar.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct IDirect3DDevice8;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
class Gui {
public:
    static LRESULT WinProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        return ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
    }
    static void Frame();
    static void Initialize() {
        if (!ImGui::GetCurrentContext())
            ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        if (!io.BackendPlatformUserData)
            ImGui_ImplWin32_Init(GetForegroundWindow());
        
        if (!io.BackendRendererUserData)
            if (shar::p3d::display->device)
                ImGui_ImplWin32_Init(shar::p3d::display->device);
    }
    static void Shutdown() {
        if (ImGui::GetCurrentContext())
            if (ImGui::GetIO().BackendRendererUserData)
                ImGui_ImplDX8_Shutdown();
    }
};
