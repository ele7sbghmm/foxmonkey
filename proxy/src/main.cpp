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
         || ctx == Ctx::LOADING_GAMEPLAY
         || ctx == Ctx::GAMEPLAY
         || ctx == Ctx::PAUSE) return true;
    }
    return false;
}

bool SliderDouble(const char* label, double* v, double v_min, double v_max,
    const char* format = "%.3f", ImGuiSliderFlags flags = 0) {
    auto tmp_v = static_cast<float>(*v);
    if (ImGui::SliderFloat(label, &tmp_v, static_cast<float>(v_min), static_cast<float>(v_max), format, flags)) {
        *v = static_cast<double>(tmp_v);
        return true;
    }
    return false;
}

void Gui::Frame() {
    if (!GoodContext())
        return;

    Initialize();

    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGui::GetIO().MouseDrawCursor = true;

    ImGui::Begin("###begin");
    ImGui::Checkbox("use fake", &fps.use_time_scale);
    SliderDouble("elapsed", &fps.time_scale, .1, 10., "%.1f", ImGuiSliderFlags_Logarithmic);
    ImGui::End();

    ImGui::Render();
    ImGui_ImplDX8_RenderDrawData(ImGui::GetDrawData());
}

