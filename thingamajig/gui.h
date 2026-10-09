#pragma once

#include <string>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx8.h"

#include "thingamajig/utility.h"

struct HWND__;
typedef HWND__* HWND;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

class Gui {
public:
	bool init = false;

	Gui() = default;
	void Init(HWND hwnd) {
		ImGui::CreateContext();
		ImGui_ImplWin32_Init(hwnd);
		ImGui_ImplDX8_Init(get_device());
		init = true;
	}
	static LRESULT WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		return ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
	}
	void Flush();

	void MainPanel();
	void ConfigPanel();
};
