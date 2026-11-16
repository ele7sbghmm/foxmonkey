#include "gooey.h"
#include <imgui.h>
#include <string>
#include <windows.h>

#include <contexts/gameplay/gameplaycontext.h>
#include <worldsim/avatarmanager.h>
#include <worldsim/avatar.h>
#include <worldsim/character/character.h>
#include <worldsim/redbrick/vehicle.h>
#include <camera/supercammanager.h>
#include <camera/supercamcentral.h>
#include <camera/supercam.h>
#include <render/RenderManager/RenderManager.h>
#include <render/RenderManager/WorldRenderLayer.h>
#include <render/Culling/WorldScene.h>

bool g_AllowSkipping = false;
bool g_SurpressNextSkipButton = false;
bool g_SurpressNextLetterbox = false;
bool g_ThrottleEnable = false;
int g_ThrottleRate = 100;


unsigned int s_cheatsEnabled = NULL;
HANDLE s_hProcess = nullptr;

void SetProcessCpuThrottle() {
	static HANDLE hJob = NULL;
	if (!hJob) {
		hJob = CreateJobObjectW(NULL, NULL);
		AssignProcessToJobObject(hJob, s_hProcess);
	}
	JOBOBJECT_CPU_RATE_CONTROL_INFORMATION cpuInfo{};
	if (g_ThrottleEnable) {
		cpuInfo.ControlFlags = JOB_OBJECT_CPU_RATE_CONTROL_ENABLE
			                 | JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP;
		cpuInfo.CpuRate = g_ThrottleRate;
	}
	else
		cpuInfo.ControlFlags = NULL;

	SetInformationJobObject(hJob, JobObjectCpuRateControlInformation, &cpuInfo, sizeof(cpuInfo));
}
void Begin() {
	if (!s_hProcess)
		s_hProcess = GetCurrentProcess();

	GameplayContext* gc = GetGameplayContext();
	AvatarManager* am = GetAvatarManager();
	Avatar* av = am ? am->GetAvatarForPlayer(0) : nullptr;
	Character* ch = av ? av->GetCharacter() : nullptr;
	Vehicle* vh = av ? av->GetVehicle() : nullptr;
	SuperCamManager* scm = GetSuperCamManager();
	SuperCamCentral* scc = scm ? scm->GetSCC(0) : nullptr;
	SuperCam* sc = scc ? scc->GetActiveSuperCam() : nullptr;
	RenderManager* rm = GetRenderManager();
	WorldRenderLayer* wrl = rm ? reinterpret_cast<WorldRenderLayer*>(rm->mpRenderLayers[2]) : nullptr;
	WorldScene* ws = wrl ? wrl->pWorldScene() : nullptr;

	char buf[256]{};
	sprintf_s(buf, sizeof(buf), "fps: %.1f###begin", ImGui::GetIO().Framerate);
	ImGui::Begin(buf);

	if (ImGui::CollapsingHeader("a")) {
		static rmt::Vector pos{}, tp{};
		if (ch && ImGui::CollapsingHeader("Character")) {
			ch->GetPosition(&pos);
			//ch->GetPosition(&tp);
			ImGui::Button("Pos"); ImGui::SameLine();
			ImGui::InputFloat3("###char_pos", (float*)&pos, "%.1f");
			if (ImGui::Button("Teleport"))
				ch->SetPosition(tp);
			ImGui::SameLine();
			ImGui::InputFloat3("###char_tp", (float*)&tp, "%.1f");
		}
		if (sc) {
			sc->GetPosition(&pos);
			ImGui::Text("Sc: %3.1f %3.1f %3.1f", pos.x, pos.y, pos.z);
		}
		if (ws) {
			ImGui::Text("mViewVector: %.1f %.1f %.1f", ws->mViewVector.x, ws->mViewVector.y, ws->mViewVector.z);
			ImGui::Text("mViewPosn: %.1f %.1f %.1f", ws->mViewPosn.x, ws->mViewPosn.y, ws->mViewPosn.z);
		}
	}
	if (ImGui::CollapsingHeader("wtw ess")) {
		ImGui::Checkbox("g_AllowSkipping", &g_AllowSkipping);
		ImGui::Checkbox("g_SurpressNextSkipButton", &g_SurpressNextSkipButton);
		ImGui::Checkbox("g_SurpressNextLetterbox", &g_SurpressNextLetterbox);

		ImGui::Text("Gameplay Context: ");
		if (gc) {
			ImGui::SameLine();
			if (ImGui::SmallButton(gc->IsSuspended() ? "Suspended" : "Active"))
				gc->IsSuspended() ? gc->Resume() : gc->Suspend();
		}
	}
	if (ImGui::CollapsingHeader("Throttle")) {
		static int s_Affinity = 1;
		ImGui::Text("Affinity: "); ImGui::SameLine();
		if (ImGui::SmallButton(s_Affinity == 1 ? "   1" : "ffff")) {
			s_Affinity = s_Affinity == 1 ? 0xffff : 1;
			SetProcessAffinityMask(s_hProcess, s_Affinity);
		}
		if (ImGui::Checkbox("Enable", &g_ThrottleEnable)
				|| ImGui::SliderInt("Rate", &g_ThrottleRate, 300, 10'000))
			SetProcessCpuThrottle();

		static DWORD s_PriorityClass = HIGH_PRIORITY_CLASS;
		ImGui::Text("Priority: "); ImGui::SameLine();
		if (ImGui::SmallButton(s_PriorityClass == IDLE_PRIORITY_CLASS ? "IDLE" : "HIGH")) {
			s_PriorityClass = s_PriorityClass == HIGH_PRIORITY_CLASS ? IDLE_PRIORITY_CLASS : HIGH_PRIORITY_CLASS;
			SetPriorityClass(s_hProcess, s_PriorityClass);
		}
		static BOOL s_PriorityBoost = TRUE;
		ImGui::Text("Boost: "); ImGui::SameLine();
		if (ImGui::SmallButton(s_PriorityBoost == TRUE ? "TRUE" : "FALSE")) {
			s_PriorityBoost = s_PriorityBoost == TRUE ? FALSE : TRUE;
			SetProcessPriorityBoost(s_hProcess, s_PriorityBoost);
		}
	}
	
	ImGui::End();
}