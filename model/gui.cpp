#include "model/gui.h"
#include "model/fps.h"

#include "gameflow/gameflow.h"
#include "render/RenderManager/RenderManager.h"

void Gui::Initialize() const {
    if (ImGui::GetCurrentContext() == nullptr)
        ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    if (io.BackendRendererUserData == nullptr) {
        auto device = reinterpret_cast<d3dDisplay*>(p3d::display)->GetD3DDevice();
        ImGui_ImplDX9_Init(device);
    }

    if (io.BackendPlatformUserData == nullptr) {
        HWND hwnd = GetForegroundWindow();
        if (IsWindow(hwnd))
            ImGui_ImplWin32_Init(hwnd);
    }
}

void Gui::Shutdown() const {
    if (ImGui::GetCurrentContext() != nullptr) {
        ImGuiIO& io = ImGui::GetIO();
        //if (io.BackendPlatformUserData != nullptr)
            //ImGui_ImplWin32_Shutdown();
        if (io.BackendRendererUserData != nullptr)
            ImGui_ImplDX9_Shutdown();
    }
}

void Gui::Frame() {
    auto* gf = GetGameFlow();
    if (!gf) return;

    auto ctx = gf->GetCurrentContext();
    if (
        ctx != CONTEXT_GAMEPLAY
        && ctx != CONTEXT_FRONTEND
        && ctx != CONTEXT_LOADING_GAMEPLAY
        && ctx != CONTEXT_FRONTEND
        ) return;

    Initialize();

    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGui::GetIO().MouseDrawCursor = true;

    BeginEnd();

    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

void Gui::BeginEnd() {
    char buf[64];
    sprintf_s(buf, sizeof(buf), "Sec: %4d   Dur: %.2f###begin", fps.total_frames_last_second, fps.target_duration.count());
    ImGui::Begin(buf);

    ImGui::Checkbox("Cap FPS", &fps.cap_fps);
    float f = fps.target_fps;
    if (ImGui::SliderFloat("FPS: ", &f, 5., 500.))
        fps.SetFPS(f);
    if (ImGui::Button("Dump")) {
        RenderManager* rm = GetRenderManager();
        if (rm)
            rm->LoadAllNeededData();
    }

    ImGui::End();
}
