#include "gui.h"

#include <stdio.h>
#include <string.h>
#include <typeinfo>

#include <radtime.hpp>
#include <meta/eventlocator.h>
#include <meta/recttriggervolume.h>
#include <worldsim/redbrick/vehicle.h>
#include <worldsim/redbrick/vehiclecontroller/vehiclecontroller.h>
#include <worldsim/vehiclecentral.h>
#include <worldsim/huskpool.h>
#include <worldsim/avatarmanager.h>
#include <worldsim/avatar.h>
#include <worldsim/character/character.h>
#include <worldsim/parkedcars/parkedcarmanager.h>
#include <mission/gameplaymanager.h>
#include <ai/actionbuttonmanager.h>
#include <ai/actionbuttonhandler.h>

bool Gui::open = false;
bool Gui::init = false;

namespace
{
    const char* VehicleTypeToString(VehicleType vt)
    {
        switch (vt)
        {
        case VT_USER:    return "USER";
        case VT_AI:      return "AI";
        case VT_TRAFFIC: return "TRAFFIC";
        default:         return "UNSET";
        }
    }

    const char* LocoTypeToString(VehicleLocomotionType vl)
    {
        switch (vl)
        {
        case VL_PHYSICS: return "PHYSICS";
        case VL_TRAFFIC: return "TRAFFIC";
        default:         return "?";
        }
    }

    // Finds the most recent record for a Vehicle* (preferring alive records)
    const Vehicle::InstanceRecord* FindRecordForPtr(const Vehicle* ptr, bool* outFoundDeletedAtSameAddr = NULL)
    {
        if (outFoundDeletedAtSameAddr)
        {
            *outFoundDeletedAtSameAddr = false;
        }
        if (!ptr)
        {
            return NULL;
        }

        const Vehicle::InstanceRecord* lastDeletedMatch = NULL;
        for (size_t i = Vehicle::s_InstanceRecordCount; i > 0; --i)
        {
            const Vehicle::InstanceRecord& rec = Vehicle::s_InstanceRecords[i - 1];
            if (rec.ptr == ptr)
            {
                if (rec.alive)
                {
                    return &rec;
                }
                if (!lastDeletedMatch)
                {
                    lastDeletedMatch = &rec;
                }
            }
        }

        if (lastDeletedMatch && outFoundDeletedAtSameAddr)
        {
            *outFoundDeletedAtSameAddr = true;
        }
        return lastDeletedMatch;
    }

    // Formats a Vehicle* into a human-readable summary using the instance tracker
    void FormatVehicleSummary(const Vehicle* v, char* buf, size_t bufSize)
    {
        if (!v)
        {
            snprintf(buf, bufSize, "NULL");
            return;
        }

        bool isDangling = false;
        const Vehicle::InstanceRecord* rec = FindRecordForPtr(v, &isDangling);
        if (rec)
        {
            if (rec->alive)
            {
                snprintf(buf, bufSize, "#%zu %s (0x%p) [VC:%d, Ref:%u, %s]",
                    rec->instanceId,
                    rec->name,
                    (const void*)v,
                    rec->lastCentralIndex,
                    rec->lastRefCount,
                    VehicleTypeToString(rec->vehicleType));
            }
            else
            {
                snprintf(buf, bufSize, "DANGLING! #%zu %s (0x%p) [DELETED!]",
                    rec->instanceId,
                    rec->name,
                    (const void*)v);
            }
        }
        else
        {
            snprintf(buf, bufSize, "0x%p (untracked)", (const void*)v);
        }
    }

    void DrawVehicleSummaryColored(const Vehicle* v, const Vehicle* compareWith = NULL)
    {
        char buf[160];
        FormatVehicleSummary(v, buf, sizeof(buf));

        if (!v)
        {
            ImGui::TextDisabled("NULL");
            return;
        }

        bool isDangling = false;
        FindRecordForPtr(v, &isDangling);
        if (isDangling)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "%s", buf);
        }
        else if (compareWith && v != compareWith)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "%s", buf);
        }
        else
        {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "%s", buf);
        }
    }

    // Collect role badges for a given live or dead Vehicle*
    void BuildVehicleRoleTags(const Vehicle* v, char* outTags, size_t maxLen)
    {
        outTags[0] = '\0';
        if (!v)
        {
            return;
        }

        auto appendTag = [&](const char* tag)
        {
            if (outTags[0] != '\0')
            {
                strncat(outTags, " ", maxLen - strlen(outTags) - 1);
            }
            strncat(outTags, tag, maxLen - strlen(outTags) - 1);
        };

        GameplayManager* gpm = GameplayManager::spInstance;
        if (gpm)
        {
            if (gpm->mCurrentVehicle == v)
            {
                appendTag("[GPM:Current]");
            }
            if (gpm->mVehicleSlots[GameplayManager::eDefaultCar].mp_vehicle == v)
            {
                appendTag("[GPM:Default]");
            }
            if (gpm->mVehicleSlots[GameplayManager::eDefaultCar].pHuskVehicle == v)
            {
                appendTag("[GPM:DefaultHusk]");
            }
            if (gpm->mVehicleSlots[GameplayManager::eOtherCar].mp_vehicle == v)
            {
                appendTag("[GPM:Other]");
            }
            if (gpm->mVehicleSlots[GameplayManager::eOtherCar].pHuskVehicle == v)
            {
                appendTag("[GPM:OtherHusk]");
            }
            if (gpm->mVehicleSlots[GameplayManager::eAICar].mp_vehicle == v)
            {
                appendTag("[GPM:AICar]");
            }
            for (int i = 0; i < GameplayManager::MAX_MISSION_VEHICLE_SLOTS; ++i)
            {
                if (gpm->mMissionVehicleSlots[i].vehicle == v)
                {
                    char tag[32];
                    snprintf(tag, sizeof(tag), "[MissionSlot:%d]", i);
                    appendTag(tag);
                }
                if (gpm->mMissionVehicleSlots[i].pHuskVehicle == v)
                {
                    char tag[32];
                    snprintf(tag, sizeof(tag), "[MissionHusk:%d]", i);
                    appendTag(tag);
                }
            }
            for (int i = 0; i < GameplayManager::MAX_VDU_CARS; ++i)
            {
                if (gpm->mVDU.mpVehicle[i] == v)
                {
                    char tag[32];
                    snprintf(tag, sizeof(tag), "[VDU:%d]", i);
                    appendTag(tag);
                }
            }
        }

        AvatarManager* am = AvatarManager::spAvatarManager;
        if (am && am->mNumAvatars > 0 && am->mAvatarArray[0])
        {
            Avatar* av = am->mAvatarArray[0];
            if (av->mpVehicle == v)
            {
                appendTag("[Avatar:Car]");
            }
            Character* ch = av->mpCharacter;
            if (ch && ch->mpTargetVehicle == v)
            {
                appendTag("[Char:Target]");
            }
        }

        ParkedCarManager* pcm = ParkedCarManager::spInstance;
        if (pcm)
        {
            if (pcm->mFreeCar.mCar == v || pcm->mFreeCar.mHusk == v)
            {
                appendTag("[PCM:FreeCar]");
            }
        }
    }

    // Finds the live Vehicle* that owns the given EventLocator*
    Vehicle* FindVehicleOwningLocator(const EventLocator* loc)
    {
        if (!loc)
        {
            return NULL;
        }
        for (size_t i = 0; i < Vehicle::s_InstanceRecordCount; ++i)
        {
            const Vehicle::InstanceRecord& rec = Vehicle::s_InstanceRecords[i];
            if (rec.alive && rec.ptr && rec.ptr->mpEventLocator == loc)
            {
                return rec.ptr;
            }
        }
        return NULL;
    }
}

void Gui::Begin()
{
    static bool s_showDeletedVehicles = true;
    static bool s_showEmptyCentralSlots = false;
    static float s_newHighlightSeconds = 5.0f;

    const unsigned int nowMs = radTimeGetMilliseconds_REAL();

    // Refresh all live vehicle records once per frame
    for (size_t i = 0; i < Vehicle::s_InstanceRecordCount; ++i)
    {
        if (Vehicle::s_InstanceRecords[i].alive && Vehicle::s_InstanceRecords[i].ptr)
        {
            Vehicle::RefreshInstanceRecord(Vehicle::s_InstanceRecords[i].ptr);
        }
    }

    VehicleCentral* vc = VehicleCentral::spInstance;
    GameplayManager* gpm = GameplayManager::spInstance;
    AvatarManager* am = AvatarManager::spAvatarManager;
    ActionButtonManager* abm = ActionButtonManager::spActionButtonManager;
    Avatar* playerAvatar = (am && am->mNumAvatars > 0) ? am->mAvatarArray[0] : NULL;
    Character* playerChar = playerAvatar ? playerAvatar->mpCharacter : NULL;

    // -------------------------------------------------------------------------
    // Top Status & Controls
    // -------------------------------------------------------------------------
    size_t deletedCount = (Vehicle::s_TotalInstanceCount >= Vehicle::s_InstanceCount)
        ? (Vehicle::s_TotalInstanceCount - Vehicle::s_InstanceCount)
        : 0;

    ImGui::Text("Live Vehicles: %zu  |  Total Created: %zu  |  Deleted: %zu  |  VehicleCentral Active: %d / %d",
        Vehicle::s_InstanceCount,
        Vehicle::s_TotalInstanceCount,
        deletedCount,
        vc ? vc->mNumActiveVehicles : 0,
        VehicleCentral::MAX_ACTIVE_VEHICLES);

    ImGui::Checkbox("Show Deleted Vehicles", &s_showDeletedVehicles);
    ImGui::SameLine();
    ImGui::Checkbox("Show Empty VC Slots", &s_showEmptyCentralSlots);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::SliderFloat("New Highlight (s)", &s_newHighlightSeconds, 1.0f, 30.0f, "%.1fs");
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear Deleted History"))
    {
        Vehicle::ClearDeletedInstanceRecords();
    }

    ImGui::Separator();

    // -------------------------------------------------------------------------
    // 1. "Who Thinks What Is Our Car?" (Manager & State Comparison)
    // -------------------------------------------------------------------------
    if (ImGui::CollapsingHeader("1. Who Thinks What Is Our Car? (Managers & Player State)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        Vehicle* gpmCurr = gpm ? gpm->mCurrentVehicle : NULL;
        Vehicle* avCar = playerAvatar ? playerAvatar->mpVehicle : NULL;
        Vehicle* charTarget = playerChar ? playerChar->mpTargetVehicle : NULL;

        // Glitch / Desync alerts
        if (playerAvatar && playerAvatar->IsInCar() && avCar != gpmCurr)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                "[GLITCH / DESYNC] Avatar is IN CAR, but Avatar->mpVehicle != GameplayManager->mCurrentVehicle!");
        }
        if (playerChar && playerChar->IsInCar() && charTarget != avCar)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                "[GLITCH / DESYNC] Character->mpTargetVehicle != Avatar->mpVehicle while in car!");
        }

        if (ImGui::BeginTable("CarOwnershipTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("System / Pointer", ImGuiTableColumnFlags_WidthFixed, 230.0f);
            ImGui::TableSetupColumn("Vehicle Instance", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Extra State / Notes", ImGuiTableColumnFlags_WidthFixed, 230.0f);
            ImGui::TableHeadersRow();

            // GameplayManager::mCurrentVehicle
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("GameplayManager::mCurrentVehicle");
            ImGui::TableSetColumnIndex(1);
            DrawVehicleSummaryColored(gpmCurr);
            ImGui::TableSetColumnIndex(2);
            if (gpm)
            {
                ImGui::Text("PhoneBooths: %s", gpm->mEnablePhoneBooths ? "ON" : "OFF");
            }
            else
            {
                ImGui::TextDisabled("No GameplayManager");
            }

            // GameplayManager Slots: Default, Other, AI
            if (gpm)
            {
                const char* slotNames[GameplayManager::MAX_VEHICLE_SLOTS] = {
                    "GPM Slot [0] eDefaultCar",
                    "GPM Slot [1] eOtherCar",
                    "GPM Slot [2] eAICar"
                };
                for (int s = 0; s < GameplayManager::MAX_VEHICLE_SLOTS; ++s)
                {
                    const GameplayManager::CarDataStruct& cd = gpm->mVehicleSlots[s];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%s", slotNames[s]);
                    ImGui::TableSetColumnIndex(1);
                    DrawVehicleSummaryColored(cd.mp_vehicle, gpmCurr);
                    ImGui::TableSetColumnIndex(2);
                    if (cd.usingHusk || cd.pHuskVehicle)
                    {
                        char huskBuf[96];
                        FormatVehicleSummary(cd.pHuskVehicle, huskBuf, sizeof(huskBuf));
                        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Name:'%s' HUSK:%s", cd.name, huskBuf);
                    }
                    else
                    {
                        ImGui::Text("Slot Name: '%s'", cd.name);
                    }
                }
            }

            // AvatarManager Player 0
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("AvatarManager (P0)::mpVehicle");
            ImGui::TableSetColumnIndex(1);
            DrawVehicleSummaryColored(avCar, gpmCurr);
            ImGui::TableSetColumnIndex(2);
            if (playerAvatar && am)
            {
                ImGui::Text("IsInCar:%s | Transition:%s",
                    playerAvatar->IsInCar() ? "YES" : "NO",
                    am->IsAvatarGettingInOrOutOfCar(0) ? "YES" : "NO");
            }
            else
            {
                ImGui::TextDisabled("No Avatar");
            }

            // Character Player 0 mpTargetVehicle
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Character (P0)::mpTargetVehicle");
            ImGui::TableSetColumnIndex(1);
            DrawVehicleSummaryColored(charTarget, gpmCurr);
            ImGui::TableSetColumnIndex(2);
            if (playerChar)
            {
                ImGui::Text("mbInCar:%s | InOrOut:%s",
                    playerChar->mbInCar ? "YES" : "NO",
                    playerChar->IsInCarOrGettingInOut() ? "YES" : "NO");
            }
            else
            {
                ImGui::TextDisabled("No Character");
            }

            // Character Current ActionButtonHandler (if touching a trigger)
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Character Active ButtonHandler");
            ImGui::TableSetColumnIndex(1);
            if (playerChar && playerChar->mpCurrentActionButtonHandler)
            {
                ActionButton::ButtonHandler* bh = playerChar->mpCurrentActionButtonHandler;
                ActionButton::GetInCar* gic = dynamic_cast<ActionButton::GetInCar*>(bh);
                if (gic)
                {
                    int vId = gic->GetVehicleId();
                    Vehicle* linkedCar = (vc && vId >= 0 && vId < VehicleCentral::MAX_ACTIVE_VEHICLES)
                        ? vc->mActiveVehicleList[vId]
                        : NULL;
                    DrawVehicleSummaryColored(linkedCar, gpmCurr);
                }
                else
                {
                    ImGui::Text("Non-Car Handler (Type=%d, 0x%p)", (int)bh->GetType(), (void*)bh);
                }
            }
            else
            {
                ImGui::TextDisabled("None (Not in trigger)");
            }
            ImGui::TableSetColumnIndex(2);
            if (playerChar && playerChar->mpCurrentActionButtonHandler)
            {
                ActionButton::GetInCar* gic = dynamic_cast<ActionButton::GetInCar*>(playerChar->mpCurrentActionButtonHandler);
                if (gic)
                {
                    Vehicle* owner = FindVehicleOwningLocator(gic->GetEventLocator());
                    int vId = gic->GetVehicleId();
                    Vehicle* linkedCar = (vc && vId >= 0 && vId < VehicleCentral::MAX_ACTIVE_VEHICLES)
                        ? vc->mActiveVehicleList[vId]
                        : NULL;
                    if (owner != linkedCar)
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                            "mVehicleId=%d (MISMATCH!)", vId);
                    }
                    else
                    {
                        ImGui::Text("GetInCar mVehicleId=%d", vId);
                    }
                }
            }

            ImGui::EndTable();
        }
    }

    // -------------------------------------------------------------------------
    // 2. GetInCar Event Triggers & ActionButtonManager Links
    // -------------------------------------------------------------------------
    if (ImGui::CollapsingHeader("2. GetInCar Event Triggers (ActionButtonManager Links)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (!abm)
        {
            ImGui::TextDisabled("ActionButtonManager not initialized.");
        }
        else
        {
            if (ImGui::BeginTable("GetInCarTriggersTable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Act ID", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                ImGui::TableSetupColumn("Player Touch?", ImGuiTableColumnFlags_WidthFixed, 95.0f);
                ImGui::TableSetupColumn("Locator / Trigger Owner Vehicle", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Trig Active", ImGuiTableColumnFlags_WidthFixed, 75.0f);
                ImGui::TableSetupColumn("mVehicleId", ImGuiTableColumnFlags_WidthFixed, 75.0f);
                ImGui::TableSetupColumn("Linked Vehicle (VC[mVehicleId])", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Link Status", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                ImGui::TableHeadersRow();

                for (int i = 0; i < ActionButtonManager::MAX_ACTIONS; ++i)
                {
                    ActionButton::ButtonHandler* bh = abm->mActionButtonList[i].handler;
                    if (!bh)
                    {
                        continue;
                    }
                    ActionButton::GetInCar* gic = dynamic_cast<ActionButton::GetInCar*>(bh);
                    if (!gic)
                    {
                        continue;
                    }

                    int actionId = abm->mActionButtonList[i].id;
                    EventLocator* loc = gic->GetEventLocator();
                    Vehicle* ownerVehicle = FindVehicleOwningLocator(loc);
                    int vId = gic->GetVehicleId();
                    bool validSlot = (vId >= 0 && vId < VehicleCentral::MAX_ACTIVE_VEHICLES);
                    Vehicle* linkedVehicle = (vc && validSlot) ? vc->mActiveVehicleList[vId] : NULL;

                    bool isCurrentCharHandler = (playerChar && playerChar->mpCurrentActionButtonHandler == bh);
                    bool isInCharHandlerList = false;
                    if (playerChar)
                    {
                        for (int h = 0; h < Character::MAX_ACTION_BUTTON_HANDLERS; ++h)
                        {
                            if (playerChar->mpActionButtonHandlers[h] == bh)
                            {
                                isInCharHandlerList = true;
                                break;
                            }
                        }
                    }

                    ImGui::TableNextRow();
                    if (isCurrentCharHandler)
                    {
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(30, 80, 50, 140));
                    }

                    // Act ID
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%d", actionId);

                    // Player Touch?
                    ImGui::TableSetColumnIndex(1);
                    if (isCurrentCharHandler)
                    {
                        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "ACTIVE (P0)");
                    }
                    else if (isInCharHandlerList)
                    {
                        ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "IN VOLUME");
                    }
                    else
                    {
                        ImGui::TextDisabled("-");
                    }

                    // Owner Vehicle
                    ImGui::TableSetColumnIndex(2);
                    if (ownerVehicle)
                    {
                        char ownerBuf[128];
                        FormatVehicleSummary(ownerVehicle, ownerBuf, sizeof(ownerBuf));
                        ImGui::Text("%s", ownerBuf);
                    }
                    else
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f),
                            "ORPHAN Locator '%s' (0x%p)",
                            loc ? loc->GetName() : "NULL",
                            (void*)loc);
                    }

                    // Trigger Active
                    ImGui::TableSetColumnIndex(3);
                    if (ownerVehicle)
                    {
                        if (ownerVehicle->mTriggerActive)
                        {
                            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "ON");
                        }
                        else
                        {
                            ImGui::TextDisabled("OFF");
                        }
                    }
                    else
                    {
                        ImGui::TextDisabled("?");
                    }

                    // mVehicleId
                    ImGui::TableSetColumnIndex(4);
                    if (validSlot)
                    {
                        ImGui::Text("Slot %d", vId);
                    }
                    else
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "%d (INV)", vId);
                    }

                    // Linked Vehicle
                    ImGui::TableSetColumnIndex(5);
                    if (linkedVehicle)
                    {
                        DrawVehicleSummaryColored(linkedVehicle, ownerVehicle);
                    }
                    else
                    {
                        ImGui::TextDisabled("NULL (Empty Slot)");
                    }

                    // Link Status
                    ImGui::TableSetColumnIndex(6);
                    if (!validSlot)
                    {
                        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.3f, 1.0f), "UNSET / NEVER ADDED");
                    }
                    else if (linkedVehicle == ownerVehicle && linkedVehicle != NULL)
                    {
                        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "OK (Matches Owner)");
                    }
                    else if (linkedVehicle == NULL)
                    {
                        if (ownerVehicle && ownerVehicle->mTriggerActive)
                        {
                            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "GLITCH: ACTIVE->NULL!");
                        }
                        else
                        {
                            ImGui::TextDisabled("Removed from VC");
                        }
                    }
                    else
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "GLITCH: WRONG CAR!");
                    }
                }

                ImGui::EndTable();
            }
        }
    }

    // -------------------------------------------------------------------------
    // 3. VehicleCentral Active Vehicle List
    // -------------------------------------------------------------------------
    if (ImGui::CollapsingHeader("3. VehicleCentral Active Vehicle List (mActiveVehicleList)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (!vc)
        {
            ImGui::TextDisabled("VehicleCentral not initialized.");
        }
        else
        {
            ImGui::Text("mNumActiveVehicles: %d / %d  |  mbVehicleTriggersActive: %s  |  UnderConstruction Head/Tail: %u / %u",
                vc->mNumActiveVehicles,
                VehicleCentral::MAX_ACTIVE_VEHICLES,
                vc->mbVehicleTriggersActive ? "true" : "false",
                vc->mCurrentVehicleUnderContructionHead,
                vc->mCurrentVehicleUnderConstructionTail);

            if (ImGui::BeginTable("VCActiveTable", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthFixed, 40.0f);
                ImGui::TableSetupColumn("Vehicle (#ID Name Ptr)", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("v->VCIdx", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("Type / Loco", ImGuiTableColumnFlags_WidthFixed, 110.0f);
                ImGui::TableSetupColumn("Ref", ImGuiTableColumnFlags_WidthFixed, 40.0f);
                ImGui::TableSetupColumn("Controller", ImGuiTableColumnFlags_WidthFixed, 160.0f);
                ImGui::TableSetupColumn("GetInCar (ActID->Slot)", ImGuiTableColumnFlags_WidthFixed, 140.0f);
                ImGui::TableSetupColumn("Manager Roles", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();

                for (int slot = 0; slot < VehicleCentral::MAX_ACTIVE_VEHICLES; ++slot)
                {
                    Vehicle* v = vc->mActiveVehicleList[slot];
                    if (!v && !s_showEmptyCentralSlots)
                    {
                        continue;
                    }

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("[%02d]", slot);

                    ImGui::TableSetColumnIndex(1);
                    if (!v)
                    {
                        ImGui::TextDisabled("<empty>");
                        continue;
                    }

                    bool isDangling = false;
                    const Vehicle::InstanceRecord* rec = FindRecordForPtr(v, &isDangling);
                    if (isDangling)
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
                            "DANGLING #%zu %s (0x%p)",
                            rec ? rec->instanceId : 0,
                            rec ? rec->name : "?",
                            (void*)v);
                        continue;
                    }

                    ImGui::Text("#%zu %s (0x%p)",
                        v->m_InstanceId,
                        v->mName ? v->mName : "<null>",
                        (void*)v);

                    // v->mVehicleCentralIndex
                    ImGui::TableSetColumnIndex(2);
                    if (v->mVehicleCentralIndex == slot)
                    {
                        ImGui::Text("%d", v->mVehicleCentralIndex);
                    }
                    else
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%d (!=%d)", v->mVehicleCentralIndex, slot);
                    }

                    // Type / Loco
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%s / %s", VehicleTypeToString(v->mVehicleType), LocoTypeToString(v->mLoco));

                    // RefCount
                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%u", v->GetRefCount());

                    // Controller
                    ImGui::TableSetColumnIndex(5);
                    VehicleController* ctrl = vc->mActiveVehicleControllerList[slot];
                    if (!ctrl)
                    {
                        ImGui::TextDisabled("NULL");
                    }
                    else if (ctrl == VehicleCentral::spGenericAI)
                    {
                        ImGui::TextDisabled("spGenericAI");
                    }
                    else
                    {
                        ImGui::Text("%s (0x%p)", typeid(*ctrl).name(), (void*)ctrl);
                    }

                    // GetInCar
                    ImGui::TableSetColumnIndex(6);
                    if (v->mpEventLocator && abm)
                    {
                        int actId = v->mpEventLocator->GetData();
                        ActionButton::GetInCar* gic = dynamic_cast<ActionButton::GetInCar*>(abm->GetActionByIndex(actId));
                        if (gic)
                        {
                            ImGui::Text("Act:%d -> Slot:%d (%s)",
                                actId,
                                gic->GetVehicleId(),
                                v->mTriggerActive ? "ON" : "OFF");
                        }
                        else
                        {
                            ImGui::Text("Act:%d (no handler)", actId);
                        }
                    }
                    else
                    {
                        ImGui::TextDisabled("None");
                    }

                    // Roles
                    ImGui::TableSetColumnIndex(7);
                    char roles[256];
                    BuildVehicleRoleTags(v, roles, sizeof(roles));
                    ImGui::TextColored(ImVec4(0.5f, 0.9f, 1.0f, 1.0f), "%s", roles);
                }

                ImGui::EndTable();
            }
        }
    }

    // -------------------------------------------------------------------------
    // 4. All Tracked Vehicle Instances (Created / New / Deleted History)
    // -------------------------------------------------------------------------
    if (ImGui::CollapsingHeader("4. All Tracked Vehicle Instances (New / Alive / Deleted)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::BeginTable("AllTrackedVehiclesTable", 10, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 260.0f)))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 42.0f);
            ImGui::TableSetupColumn("State / Age", ImGuiTableColumnFlags_WidthFixed, 115.0f);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 85.0f);
            ImGui::TableSetupColumn("Ref", ImGuiTableColumnFlags_WidthFixed, 38.0f);
            ImGui::TableSetupColumn("VC Slot", ImGuiTableColumnFlags_WidthFixed, 55.0f);
            ImGui::TableSetupColumn("Type / Loco", ImGuiTableColumnFlags_WidthFixed, 105.0f);
            ImGui::TableSetupColumn("HP", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Trigger (Act->VC)", ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGui::TableSetupColumn("Flags & Manager Refs", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            // Iterate newest first so newly created/deleted cars are immediately visible at the top
            for (size_t idx = Vehicle::s_InstanceRecordCount; idx > 0; --idx)
            {
                const Vehicle::InstanceRecord& rec = Vehicle::s_InstanceRecords[idx - 1];
                if (!rec.alive && !s_showDeletedVehicles)
                {
                    continue;
                }

                float ageSec = (nowMs >= rec.createdTick) ? ((nowMs - rec.createdTick) / 1000.0f) : 0.0f;
                float delSec = (!rec.alive && nowMs >= rec.deletedTick) ? ((nowMs - rec.deletedTick) / 1000.0f) : 0.0f;
                bool isNew = rec.alive && (ageSec <= s_newHighlightSeconds);
                bool isRecentlyDeleted = !rec.alive && (delSec <= s_newHighlightSeconds);

                ImGui::TableNextRow();
                if (isNew)
                {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(20, 90, 30, 140));
                }
                else if (isRecentlyDeleted)
                {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(110, 25, 25, 140));
                }

                // ID
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("#%zu", rec.instanceId);

                // State / Age
                ImGui::TableSetColumnIndex(1);
                if (rec.alive)
                {
                    if (isNew)
                    {
                        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "NEW (%.1fs)", ageSec);
                    }
                    else
                    {
                        ImGui::Text("ALIVE (%.0fs)", ageSec);
                    }
                }
                else
                {
                    if (isRecentlyDeleted)
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "DEL (-%.1fs)", delSec);
                    }
                    else
                    {
                        ImGui::TextDisabled("DEL (-%.0fs)", delSec);
                    }
                }

                // Name
                ImGui::TableSetColumnIndex(2);
                if (rec.alive)
                {
                    ImGui::Text("%s", rec.name);
                }
                else
                {
                    ImGui::TextDisabled("%s", rec.name);
                }

                // Address
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("0x%p", (void*)rec.ptr);

                // RefCount
                ImGui::TableSetColumnIndex(4);
                if (rec.alive)
                {
                    ImGui::Text("%u", rec.lastRefCount);
                }
                else
                {
                    ImGui::TextDisabled("0");
                }

                // VC Slot
                ImGui::TableSetColumnIndex(5);
                if (rec.lastCentralIndex >= 0)
                {
                    ImGui::Text("[%d]", rec.lastCentralIndex);
                }
                else
                {
                    ImGui::TextDisabled("-1");
                }

                // Type / Loco
                ImGui::TableSetColumnIndex(6);
                ImGui::Text("%s/%s", VehicleTypeToString(rec.vehicleType), LocoTypeToString(rec.locoType));

                // HP
                ImGui::TableSetColumnIndex(7);
                if (rec.vehicleDestroyed)
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "DEAD (0)");
                }
                else
                {
                    ImGui::Text("%.2f/%.2f", rec.hitPoints, rec.maxHitPoints);
                }

                // Trigger
                ImGui::TableSetColumnIndex(8);
                if (rec.actionButtonId >= 0)
                {
                    ImGui::Text("A:%d->VC:%d(%s)",
                        rec.actionButtonId,
                        rec.lastGetInCarVehicleId,
                        rec.triggerActive ? "ON" : "OFF");
                }
                else
                {
                    ImGui::TextDisabled("-");
                }

                // Flags & Manager Refs
                ImGui::TableSetColumnIndex(9);
                char roles[256];
                BuildVehicleRoleTags(rec.ptr, roles, sizeof(roles));
                if (!rec.alive && roles[0] != '\0')
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "DANGLING REF: %s", roles);
                }
                else
                {
                    if (rec.playerCar)
                    {
                        strncat(roles, " [PlayerCar]", sizeof(roles) - strlen(roles) - 1);
                    }
                    if (rec.userDriving)
                    {
                        strncat(roles, " [UserDriving]", sizeof(roles) - strlen(roles) - 1);
                    }
                    if (rec.createdByParkedCarManager)
                    {
                        strncat(roles, " [ParkedCar]", sizeof(roles) - strlen(roles) - 1);
                    }
                    ImGui::Text("%s", roles);
                }
            }

            ImGui::EndTable();
        }
    }

    // -------------------------------------------------------------------------
    // 5. Mission Vehicles, VDU (Vehicle Disposal Unit), & HuskPool
    // -------------------------------------------------------------------------
    if (ImGui::CollapsingHeader("5. Mission Vehicle Slots, VDU Queue, & HuskPool"))
    {
        if (gpm)
        {
            ImGui::Text("GameplayManager::mMissionVehicleSlots:");
            for (int i = 0; i < GameplayManager::MAX_MISSION_VEHICLE_SLOTS; ++i)
            {
                const GameplayManager::MissionCarDataStruct& ms = gpm->mMissionVehicleSlots[i];
                if (ms.vehicle || ms.pHuskVehicle || ms.name[0] != '\0')
                {
                    char vBuf[128], hBuf[128];
                    FormatVehicleSummary(ms.vehicle, vBuf, sizeof(vBuf));
                    FormatVehicleSummary(ms.pHuskVehicle, hBuf, sizeof(hBuf));
                    ImGui::BulletText("MissionSlot[%d] '%s': %s | usingHusk:%s (%s)",
                        i, ms.name, vBuf, ms.usingHusk ? "YES" : "NO", hBuf);
                }
            }

            ImGui::Separator();
            ImGui::Text("GameplayManager::mVDU (Vehicle Disposal Unit - Counter: %d / %d):",
                gpm->mVDU.mCounter, GameplayManager::MAX_VDU_CARS);
            for (int i = 0; i < GameplayManager::MAX_VDU_CARS; ++i)
            {
                if (gpm->mVDU.mpVehicle[i])
                {
                    char vBuf[128];
                    FormatVehicleSummary(gpm->mVDU.mpVehicle[i], vBuf, sizeof(vBuf));
                    ImGui::BulletText("VDU[%d]: %s", i, vBuf);
                }
            }
        }

        if (vc)
        {
            ImGui::Separator();
            ImGui::Text("VehicleCentral::mHuskPool (Total: %d):", vc->mHuskPool.mTotalNum);
            for (int i = 0; i < vc->mHuskPool.mTotalNum; ++i)
            {
                const HuskPool::HuskData& hd = vc->mHuskPool.mHuskArray[i];
                char hBuf[128], oBuf[128];
                FormatVehicleSummary(hd.huskVehicle, hBuf, sizeof(hBuf));
                FormatVehicleSummary(hd.originalVehicle, oBuf, sizeof(oBuf));
                ImGui::BulletText("Husk[%d] inUse:%s | Husk: %s | Orig: %s",
                    i, hd.inUse ? "YES" : "NO", hBuf, oBuf);
            }
        }
    }
}
