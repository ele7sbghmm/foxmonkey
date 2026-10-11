#pragma once

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx8.h"

#include <windows.h>

struct IDirect3DDevice8;

class Gui {
public:
    LRESULT WinProc(HWND, UINT, WPARAM, LPARAM);
    void Frame() { }
    void Initialize() {
        if (!ImGui::GetCurrentContext())
            ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        if (!io.BackendPlatformUserData)
            ImGui_ImplWin32_Init(GetForegroundWindow());
        
        if (!io.BackendRendererUserData)
    }
    void Shutdown() { }
};
