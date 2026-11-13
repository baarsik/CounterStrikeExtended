#include "LanguagePresentation.h"
#include "CareerBaseFrame.h"
#include "CareerBox.h"
#include "CareerDefinitions.h"
#include "CareerGameTasks.h"
#include "CareerMapFrame.h"
#include "CareerProfileFrame.h"
#include "LoadingDialog.h"
#include "DemoPlayerDialog.h"
#include "OptionsSubKeyboard.h"
#include "PlayerListDialog.h"
#include "Taskbar.h"
#include "VGuiSystemModuleLoader.h"
#include "Label.h"
#include "Frame.h"
#include "ComboBox.h"
#include "PHandle.h"
#include "localization_controls.h"
#if INTPTR_MAX > INT32_MAX
#include "native/ServerBrowserDialog.h"
#endif
#include <vector>

void RefreshCareerBotLanguagePresentation(vgui2::Panel* panel);
void RefreshCareerMapButtonLanguagePresentation(vgui2::Panel* panel);

namespace cse::gameui {
namespace {
void RefreshTasks() {
    if (!TheCareerDefinitions) {
        return;
    }
    for (int career = 0; career < TheCareerDefinitions->GetNumCareers(); ++career) {
        CareerDefinition* definition = TheCareerDefinitions->GetCareer(career);
        if (!definition) {
            continue;
        }
        for (int difficulty = 0; difficulty < MAX_CAREER_DIFFICULTY; ++difficulty) {
            MapsVec* maps = definition->GetDifficulty(static_cast<CareerDifficultyType>(difficulty))->GetMaps();
            for (auto& triplet : *maps) {
                for (CCareerMap* map : triplet) {
                    for (ICareerTask* task : *map->GetTasks()) {
                        if (auto* owned = dynamic_cast<CCareerUITask*>(task)) {
                            owned->RefreshLanguagePresentation();
                        }
                    }
                }
            }
        }
    }
}
}

void RefreshLanguagePresentation(vgui2::Panel* root, std::uint64_t revision) {
    (void)revision;
    RefreshTasks();
    std::vector<vgui2::DHANDLE<vgui2::Panel>> panels;
    if (root) {
        vgui2::DHANDLE<vgui2::Panel> handle;
        handle = root;
        panels.push_back(handle);
    }
    // Snapshot weak handles before invoking owners. Descendants created during a
    // refresh already use the committed catalogue; retired panels are skipped.
    for (std::size_t index = 0; index < panels.size(); ++index) {
        if (vgui2::Panel* panel = panels[index].Get()) {
            for (int child = 0; child < panel->GetChildCount(); ++child) {
                vgui2::DHANDLE<vgui2::Panel> handle;
                handle = panel->GetChild(child);
                panels.push_back(handle);
            }
        }
    }
    for (auto& handle : panels) {
        if (vgui2::Panel* panel = handle.Get()) {
            if (auto* frame = dynamic_cast<vgui2::Frame*>(panel)) {
                frame->RefreshLanguageText();
            }
            if (auto* combo = dynamic_cast<vgui2::ComboBox*>(panel)) {
                combo->RefreshLanguageText();
            }
            if (auto* label = dynamic_cast<vgui2::Label*>(panel)) {
                cse::localization::RefreshText(label->GetTextImage());
            }
        }
    }
    for (auto& handle : panels) {
        vgui2::Panel* panel = handle.Get();
        if (!panel) {
            continue;
        }
        if (auto* owner = dynamic_cast<CCareerBaseFrame*>(panel)) {
            owner->RefreshLanguagePresentation();
        }
        if (auto* owner = dynamic_cast<CCareerMapFrame*>(panel)) {
            owner->RefreshLanguagePresentation();
        }
        if (auto* owner = dynamic_cast<CCareerProfileFrame*>(panel)) {
            owner->RefreshLanguagePresentation();
        }
        if (auto* owner = dynamic_cast<CCareerQueryBox*>(panel)) {
            owner->RefreshLanguagePresentation();
        }
        if (auto* owner = dynamic_cast<CCareerDifficultyBox*>(panel)) {
            owner->RefreshLanguagePresentation();
        }
        if (auto* owner = dynamic_cast<CDemoPlayerDialog*>(panel)) {
            owner->RefreshLanguagePresentation();
        }
        if (auto* owner = dynamic_cast<CLoadingDialog*>(panel)) {
            owner->RefreshLanguagePresentation();
        }
        if (auto* owner = dynamic_cast<COptionsSubKeyboard*>(panel)) {
            owner->RefreshLanguagePresentation();
        }
        if (auto* owner = dynamic_cast<CPlayerListDialog*>(panel)) {
            owner->RefreshLanguagePresentation();
        }
        if (auto* owner = dynamic_cast<CTaskbar*>(panel)) {
            owner->RefreshLanguagePresentation();
        }
#if INTPTR_MAX > INT32_MAX
        if (auto* owner = dynamic_cast<cse::server_browser::ServerBrowserDialog*>(panel)) {
            owner->RefreshLanguagePresentation();
        }
#endif
        if (auto* button = dynamic_cast<CCareerButton*>(panel)) {
            button->RefreshLanguagePresentation();
        }
        RefreshCareerMapButtonLanguagePresentation(panel);
        RefreshCareerBotLanguagePresentation(panel);
        if (auto* label = dynamic_cast<vgui2::Label*>(panel)) {
            cse::localization::RefreshText(label->GetTextImage());
        }
        panel->InvalidateLayout(false, false);
    }
    g_VModuleLoader.RefreshLanguagePresentation();
}
}
