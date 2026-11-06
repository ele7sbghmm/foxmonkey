#include "gui/main.h"

#include "contexts/gameplay/gameplaycontext.h"
#include "main/commandlineoptions.h"
#include "worldsim/avatar.h"
#include "worldsim/avatarmanager.h"
#include "worldsim/character/character.h"
#include "worldsim/hitnrunmanager.h"
#include "worldsim/redbrick/vehicle.h"
#include "worldsim/vehiclecentral.h"

#include <string>
#include <vector>

enum ContextState { ACTIVE = 2, SUSPENDED = 3 };

struct CLO {
  int i;
  std::string s;
};
std::vector<CLO> Clos = {
    CLO{22, "SHOW SPEED"},          CLO{28, "NO TRAFFIC"}, CLO{39, "SHOW DYNA ZONES"},
    CLO{40, "MANUAL RESET DAMAGE"}, CLO{41, "NO PEDS"},
};

void Destroy() {
  auto *vc = GetVehicleCentral();
  if (vc) {
    Vehicle **l = nullptr;
    int n = 0;
    vc->GetActiveVehicleList(l, n);
    for (int i = 0; i < n; i++) {
      auto *v = l[i];
      if (v) {
        v->mHitPoints = 0.f;
        v->mVehicleDestroyed = true;
        v->ActivateTriggers(false);
        // v->mDamageOutResetTimer = 0.0f;
      }
    }
  }
}

std::string ContextState() {
  auto *gc = GetGameplayContext();
  if (gc)
    return gc->IsSuspended() ? "SUSPENDED" : "ACTIVE";
  return "NULL";
}

void Gui::Draw() {
  AvatarManager *am = GetAvatarManager();
  Avatar *a = am ? am->GetAvatarForPlayer(0) : nullptr;
  Character *c = a ? a->GetCharacter() : nullptr;

  auto *gc = GetGameplayContext();
  if (ImGui::Button("Suspend"))
    if (gc)
      gc->Suspend();
  if (ImGui::Button("Resume"))
    if (gc)
      gc->Resume();

  char b[64];
  if (gc)
    sprintf_s(b, sizeof(b), "%s", gc->IsSuspended() ? "SUSPENDED" : "ACTIVE");
  else
    sprintf_s(b, sizeof(b), "NULL");
  ImGui::Text(b);

  float f = 0.f;
  auto *hnr = GetHitnRunManager();
  if (hnr)
    f = hnr->GetHitnRunValue();
  if (ImGui::SliderFloat("HitnRun", &f, 0.f, 100.f, "%.1f"))
    if (hnr)
      hnr->SetHitnRunValue(f);

  if (ImGui::Button("Destroy"))
    Destroy();

  int char_index = c ? c->GetCollisionAreaIndex() : -2;
  if (ImGui::DragInt("Char Index", &char_index, -1, 31))
    if (c)
      c->mCollisionAreaIndex = char_index;

  for (auto &clo : Clos) {
    auto flag = 1ULL << static_cast<uint64_t>(clo.i);
    bool b = (CommandLineOptions::sOptions & flag) != 0;
    if (ImGui::Checkbox(clo.s.c_str(), &b)) {
      if (b)
        CommandLineOptions::sOptions |= flag;
      else
        CommandLineOptions::sOptions &= ~flag;
    }
  }
}
