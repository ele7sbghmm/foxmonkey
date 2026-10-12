#include "main.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx8.h"

void Gui::Frame() {
    Initialize();

    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("###begin");
    ImGui::End();

    ImGui::Render();
    ImGui_ImplDX8_RenderDrawData(ImGui::GetDrawData());
}
