#pragma once

#include <windows.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_dx8.h>
#include <backends/imgui_impl_win32.h>

#include <p3d/utility.hpp>
#include <pddi/dx8/display.hpp>
#include <main/game.h>

struct IDirect3DDevice8;
struct ImGui_ImplWin32_Data;

extern ImGui_ImplWin32_Data* ImGui_ImplWin32_GetBackendData();
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
class Gui {
public:
    Gui() = default;
    static bool WndProc(HWND, UINT, WPARAM, LPARAM);
    void Initialize() const;
    void Shutdown() const;
    void Frame();
    void BeginEnd();
};

extern Gui gui;
