#include "thingamajig/gui.h"
#include "thingamajig/globals.h"


void Gui::Flush() {
	if (!init)
		return;

	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::GetIO().MouseDrawCursor = true;

	char label[1024]{};
	sprintf_s(label, sizeof(label), "fps: %d (%.2f ms)###begin",
		g_frameLimiter.GetFramesInLastSecond(),
		g_frameLimiter.GetLastTotalFrameTimeMs()
		);
	ImGui::Begin(label);

	MainPanel();

	ImGui::End();
	ImGui::Render();

	ImGui_ImplDX8_RenderDrawData(ImGui::GetDrawData());
}
void Gui::MainPanel() {
	ConfigPanel();
}
void Gui::ConfigPanel() {
	if (ImGui::TreeNode("Config")) {
		if (ImGui::InputDouble("fps", (double*)&g_fps, 1., 1., "%.0f"))
		{ }
			//g_frameLimiter.SetTargetFps(g_fps);
		if (ImGui::InputDouble("fps g", (double*)&g_fps_gameplay, 1., 1., "%.0f"))
			g_frameLimiter.SetTargetFps(g_fps_gameplay);

		ImGui::TreePop();
	}
}
