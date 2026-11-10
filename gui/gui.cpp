#include "gui/gui.h"

#include <windows.h>
#include <d3d8.h>
#include <string>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx8.h"
#include "draw/fences/fences.h"
#include "fps/fps.h"

#include "worldsim/hitnrunmanager.h"
#include "contexts/gameplay/gameplaycontext.h"
#include "presentation/gui/guisystem.h"
#include "presentation/gui/ingame/guimanageringame.h"
#include "camera/animatedcam.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace srr2
{
static bool g_Initialized = false;
static bool g_ShowWindow = true;
static bool g_ShowDemoWindow = false;
static float g_TestFloat = 0.5f;
static float g_TintColor[3] = { 0.25f, 0.55f, 0.90f };
static int g_ClickCount = 0;
static int g_ConsoleLogs = 0;

bool Gui::Init(void* hwnd, IDirect3DDevice8* device) {
    AllocConsole();
    FILE* f{};
    freopen_s(&f, "CONOUT$", "w", stdout);

    if (g_Initialized)
        return true;

    if (!hwnd || !device)
        return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.MouseDrawCursor = true;

    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(hwnd))
    {
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplDX8_Init(device))
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    g_Initialized = true;
    return true;
}

void Gui::Shutdown()
{
    if (!g_Initialized)
        return;

    ImGui_ImplDX8_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_Initialized = false;
}

void Gui::OnPreReset()
{
    if (!g_Initialized)
        return;

    ImGui_ImplDX8_InvalidateDeviceObjects();
}

void Gui::OnPostReset()
{
    if (!g_Initialized)
        return;

    ImGui_ImplDX8_CreateDeviceObjects();
}

bool Gui::WndProc(void* hwnd, unsigned int msg, unsigned int wParam, long lParam)
{
    if (!g_Initialized)
        return false;

    if (ImGui_ImplWin32_WndProcHandler(static_cast<HWND>(hwnd), msg, static_cast<WPARAM>(wParam), static_cast<LPARAM>(lParam)))
        return true;

    return false;
}

void Gui::Render()
{
    if (!g_Initialized)
        return;

    ImGui_ImplDX8_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (g_ShowWindow)
    {
        ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320.0f, 200.0f), ImGuiCond_FirstUseEver);

        char buf[64];
        sprintf_s(buf, sizeof(buf), "FPS: %.1f###begin", ImGui::GetIO().Framerate);
        if (ImGui::Begin(buf, &g_ShowWindow)) {
            //ImGui::Checkbox("Show ImGui Demo Window", &g_ShowDemoWindow);
            //ImGui::Checkbox("FPS Limiter", srr2::fps::GetEnabledPtr());
            //if (srr2::fps::IsEnabled()) ImGui::SliderInt("Target FPS", srr2::fps::GetTargetFpsPtr(), 5, 240);
            //ImGui::Checkbox("Draw Fences", srr2::draw::fences::GetEnabledPtr());

            auto* hnr = GetHitnRunManager();
            if (hnr) {
                auto hnrValue = hnr->GetHitnRunValue();
                if (ImGui::SliderFloat("Hit'n'Run", &hnrValue, 0.f, 100.f, "%.1f"))
                    hnr->SetHitnRunValue(hnrValue);
            }

            auto* gc = GetGameplayContext();
            if (gc) {
                ImGui::Text(gc->IsSuspended() ? "Gameplay Context: Suspended" : "Gameplay Context: Active"); ImGui::SameLine();
                if (ImGui::Button("RESUME")) gc->Resume(); ImGui::SameLine();
                if (ImGui::Button("SUSPENDED")) gc->Suspend();
            }
            ImGui::Checkbox("g_SurpressNextLetterbox", &g_SurpressNextLetterbox);
        }
        ImGui::End();
    }

    if (g_ShowDemoWindow)
    {
        ImGui::ShowDemoWindow(&g_ShowDemoWindow);
    }

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX8_RenderDrawData(ImGui::GetDrawData());
}
} // namespace srr2
