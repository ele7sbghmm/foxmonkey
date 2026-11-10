#pragma once

struct IDirect3DDevice8;

namespace srr2 {
	class Gui {
	public:
		static bool Init(void* hwnd, IDirect3DDevice8* device);
		static void Shutdown();

		static void OnPreReset();
		static void OnPostReset();

		// Returns true if ImGui handled/wants to capture the message.
		static bool WndProc(void* hwnd, unsigned int msg, unsigned int wParam, long lParam);

		static void Render();
	}; // namespace gui
} // namespace srr2
