#pragma once

#include "imgui.h"
#include "imgui_impl_dx8.h"
#include "imgui_impl_win32.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif 
#include <windows.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class Gui {
public:
	static bool open;
	static bool init;

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
	}
	static bool Init(HWND hWnd, IDirect3DDevice8* device) {
		if (init)
        	return true;

		if (!hWnd || !device)
			return false;

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		io.MouseDrawCursor = true;

		ImGui::StyleColorsDark();

		if (!ImGui_ImplWin32_Init(hWnd)) {
			ImGui::DestroyContext();
			return false;
		}

		if (!ImGui_ImplDX8_Init(device)) {
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();
			return false;
		}

		init = true;
		return true;
	}
	static void ResetDevice() {
		ImGui_ImplDX8_InvalidateDeviceObjects();
		ImGui_ImplDX8_CreateDeviceObjects();
	}
	static void Shutdown() {
		ImGui_ImplDX8_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		init = false;
	}
	static void Draw() {
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		char buf[256];
		sprintf_s(buf, sizeof(buf), "FPS: %.1f###begin", ImGui::GetIO().Framerate);
		ImGui::Begin(buf, &open);
		Begin();
		ImGui::End();

		ImGui::EndFrame();
		ImGui::Render();
		ImGui_ImplDX8_RenderDrawData(ImGui::GetDrawData());
	}
	static void Begin();
};
