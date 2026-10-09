#include "moxingyi/tuxingyonghujiemian.h"
#include "moxingyi/quanju.h"
#include "moxingyi/jishiqi.h"

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include "pddi/dx8/display.hpp"
#include "p3d/utility.hpp"

#include <string>

template <typename T> static bool Drag(const char* label, ImGuiDataType data_type, T* p_data, float v_speed,
        const T p_min, const T p_max, const char* format = static_cast<const char*>(0), ImGuiSliderFlags flags = 0) {
    return ImGui::DragScalar(label, data_type, p_data, v_speed, &p_min, &p_max, format, flags);
}
template <typename T> static bool Slider(const char* label, ImGuiDataType data_type, T* p_data,
        const T p_min, const T p_max, const char* format = static_cast<const char*>(0), ImGuiSliderFlags flags = 0) {
    return ImGui::SliderScalar(label, data_type, p_data, &p_min, &p_max, format, flags);
}

extern long ImGui_ImplWin32_WndProcHandler(HWND hWnd, unsigned int msg, unsigned int wParam, long lParam);
long 图形用户界面类::WndProc(HWND hWnd, unsigned int msg, unsigned int wParam, long lParam) {
    return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
}
void 图形用户界面类::初始化(HWND hWnd) {
    if (!已初始化) {
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(hWnd);
        已初始化 = true;
    }
    if (device_已初始化)
        ImGui_ImplDX9_Shutdown();
        
    ImGui_ImplDX9_Init(reinterpret_cast<d3dDisplay*>(p3d::display)->GetD3DDevice());
    device_已初始化 = true;
}
void 图形用户界面类::祯() {
    if (!已初始化) return;

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGui::GetIO().MouseDrawCursor = true;

    char buf[1024]{};
    sprintf_s(buf, sizeof(buf), "fps: %4.0f  fls: %4.0f###begin", timer.interpolated_fps, timer.frames_last_second);
    ImGui::Begin(buf);
    
    ImGui::Checkbox("Cap FPS", &timer.cap_fps);
    if (Slider<double>("Target FPS", ImGuiDataType_Double, &timer.fps, 1., 1000., 0, ImGuiSliderFlags_Logarithmic))
        timer.SetTargetFps(timer.fps);
    ImGui::Checkbox("Use Fake Time", &timer.use_fake_time);
    Drag<unsigned int>("Fake Delta Time", ImGuiDataType_U32, &timer.fake_elapsed, 1.f, 0, 10000, 0, ImGuiSliderFlags_Logarithmic);
    
    ImGui::End();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}
