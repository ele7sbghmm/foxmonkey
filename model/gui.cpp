#include "model/gui.h"
#include "model/fps.h"

#include "gameflow/gameflow.h"
#include "render/RenderManager/RenderManager.h"

Gui gui{};

bool Gui::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui::GetCurrentContext() == nullptr)
        return false;

    ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
    return ImGui::GetIO().WantCaptureMouse;
}
void Gui::Initialize() const {
    if (ImGui::GetCurrentContext() == nullptr)
        ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    if (io.BackendRendererUserData == nullptr) {
        auto device = reinterpret_cast<d3dDisplay*>(p3d::display)->GetD3DDevice();
        ImGui_ImplDX8_Init(device);
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
            ImGui_ImplDX8_Shutdown();
    }
}

void Gui::Frame() {
    auto* gf = GetGameFlow();
    if (gf != nullptr) {
        ContextEnum ctx = gf->GetCurrentContext();
        if (ctx != CONTEXT_GAMEPLAY
            && ctx != CONTEXT_FRONTEND
            && ctx != CONTEXT_LOADING_GAMEPLAY
            && ctx != CONTEXT_FRONTEND
            ) return;
    }

    Initialize();

    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGui::GetIO().MouseDrawCursor = true;

    BeginEnd();

    ImGui::Render();
    ImGui_ImplDX8_RenderDrawData(ImGui::GetDrawData());
}

void Gui::BeginEnd() {
    char buf[64];
    sprintf_s(buf, sizeof(buf), "Sec: %4d   Dur: %.2f###begin", fps.total_frames_last_second, fps.target_duration.count());
    ImGui::Begin(buf);

    ImGui::Checkbox("Fake: ", &fps.use_fake_elapsed);
    ImGui::SliderInt("Step: ", (int*)&fps.step, 1, 20);

    ImGui::Checkbox("Cap FPS: ", &fps.cap_fps);
    if (ImGui::SliderFloat("FPS: ", &fps.target_fps, 1., 500.))
        fps.SetFPS(fps.target_fps);
    
    if (ImGui::Button("Dump")) {
        RenderManager* rm = GetRenderManager();
        if (rm)
            rm->LoadAllNeededData();
    }

    ImGui::End();
}
