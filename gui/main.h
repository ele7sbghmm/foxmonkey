#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include <windows.h>

#include "p3d/utility.hpp"
#include "pddi/dx8/display.hpp"

struct HWND__;
typedef HWND__ *HWND;
struct IDirect3DDevice8;

class Gui {
public:
  static void Draw();
  static void NewFrame() {
    if (ImGui::GetCurrentContext() == nullptr)
      ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    if (io.BackendPlatformUserData == nullptr)
      ImGui_ImplWin32_Init(GetForegroundWindow());

    if (io.BackendRendererUserData == nullptr)
      ImGui_ImplDX9_Init(reinterpret_cast<d3dDisplay *>(p3d::display)->GetD3DDevice());

    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    io.MouseDrawCursor = true;

    ImGui::Begin("###begin");
    Draw();
    ImGui::End();

    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
  }
};
