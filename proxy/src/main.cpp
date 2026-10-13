#include "main.h"
#include "fps.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx8.h"

bool GoodContext() {
    using Ctx = shar::ContextEnum;
    if (auto* gf = shar::GameFlow::get(); gf) {
        auto ctx = gf->mCurrentContext;
        if (ctx == Ctx::FRONTEND
         || ctx == Ctx::GAMEPLAY
         || ctx == Ctx::PAUSE) return true;
    }
    return false;

}

void Gui::Frame() {
    if (!GoodContext())
        return;

    Initialize();

    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("###begin");
    ImGui::Checkbox("use fake elapsed", &fps.use_fake_elapsed);
    ImGui::SliderInt("elapsed", (int*)&fps.fake_elapsed, 1, 100);
    ImGui::End();

    ImGui::Render();
    ImGui_ImplDX8_RenderDrawData(ImGui::GetDrawData());
}
