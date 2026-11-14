#include "ServerBrowserSettingsDialog.h"
#include "ServerBrowserDialog.h"
#include "Button.h"
#include "Label.h"
#include "ListPanel.h"
#include "TextEntry.h"
#include "vgui/KeyValues.h"
#include <cstring>

namespace cse::server_browser {
namespace {
std::string EntryText(vgui2::TextEntry& entry, size_t capacity) {
    std::vector<char> bytes(capacity);
    entry.GetText(bytes.data(), static_cast<int>(bytes.size()));
    return bytes.data();
}
}
BrowserSettingsDialog::BrowserSettingsDialog(vgui2::Panel* parent, BrowserSession& session,
    std::function<void(const MasterserverSettings&)> submit)
    : Frame(parent, "BrowserConfiguration", false, false), session_(session), draft_(session.settings.masterservers),
      submit_(std::move(submit)) {
    constexpr int DialogWidth = 560;
    constexpr int DialogHeight = 360;
    constexpr int Margin = 12;
    constexpr int ContentWidth = DialogWidth - 2 * Margin;
    constexpr int LabelHeight = 20;
    constexpr int HeadingTop = 32;
    constexpr int ListTop = 56;
    constexpr int ListHeight = 144;
    constexpr int NameColumnWidth = 125;
    constexpr int AddressColumnWidth = 375;
    constexpr int FieldsTop = 208;
    constexpr int FieldStep = 30;
    constexpr int FieldLabelWidth = 116;
    constexpr int FieldLeft = 132;
    constexpr int FieldWidth = 300;
    constexpr int FieldHeight = 22;
    constexpr int ButtonWidth = 104;
    constexpr int ButtonHeight = 24;
    constexpr int ButtonGap = 8;
    constexpr int RightButtonLeft = DialogWidth - Margin - ButtonWidth;
    constexpr int HelpTop = 266;
    constexpr int ErrorTop = 290;
    constexpr int FooterTop = DialogHeight - Margin - ButtonHeight;
    SetSize(DialogWidth, DialogHeight);
    SetSizeable(false);
    SetDeleteSelfOnClose(false);
    SetTitle("#CSEServerBrowser_Configuration", true);
    auto* heading = new vgui2::Label(this, "MasterLabel", "#CSEServerBrowser_Masterservers");
    heading->SetBounds(Margin, HeadingTop, ContentWidth, LabelHeight);
    list_ = new vgui2::ListPanel(this, "Masterservers");
    list_->SetBounds(Margin, ListTop, ContentWidth, ListHeight);
    list_->SetMultiSelectEnabled(false);
    list_->AddColumnHeader(0, "Name", "#CSEServerBrowser_MasterName", NameColumnWidth,
        vgui2::ListPanel::COLUMN_FIXEDSIZE);
    list_->AddColumnHeader(1, "Addresses", "#CSEServerBrowser_MasterAddresses", AddressColumnWidth,
        vgui2::ListPanel::COLUMN_RESIZEWITHWINDOW);
    list_->AddActionSignalTarget(this);
    auto* nameLabel = new vgui2::Label(this, "NameLabel", "#CSEServerBrowser_MasterName");
    nameLabel->SetBounds(Margin, FieldsTop, FieldLabelWidth, LabelHeight);
    name_ = new vgui2::TextEntry(this, "MasterName");
    name_->SetBounds(FieldLeft, FieldsTop, FieldWidth, FieldHeight);
    name_->SetAllowNonAsciiCharacters(true);
    name_->SetMaximumCharCount(static_cast<int>(MaximumMasterNameBytes));
    name_->SetTabPosition(2);
    auto* addressLabel = new vgui2::Label(this, "AddressLabel", "#CSEServerBrowser_MasterAddresses");
    addressLabel->SetBounds(Margin, FieldsTop + FieldStep, FieldLabelWidth, LabelHeight);
    addresses_ = new vgui2::TextEntry(this, "MasterAddresses");
    addresses_->SetBounds(FieldLeft, FieldsTop + FieldStep, FieldWidth, FieldHeight);
    addresses_->SetMaximumCharCount(static_cast<int>(MaximumMasterAddressBytes));
    addresses_->SetTabPosition(3);
    auto* help = new vgui2::Label(this, "AddressHelp", "#CSEServerBrowser_MasterAddressHelp");
    help->SetBounds(Margin, HelpTop, ContentWidth, LabelHeight);
    const auto button = [this](const char* name, const char* label, const char* command,
                              int x, int y, int width, int tab) {
        auto* control = new vgui2::Button(this, name, label);
        control->SetBounds(x, y, width, ButtonHeight);
        control->SetCommand(command);
        control->AddActionSignalTarget(this);
        control->SetTabPosition(tab);
        return control;
    };
    button("AddMasterserver", "#CSEServerBrowser_AddMasterserver", "AddMasterserver",
        RightButtonLeft, FieldsTop, ButtonWidth, 4);
    remove_ = button("RemoveMasterserver", "#CSEServerBrowser_RemoveMasterserver", "RemoveMasterserver",
        RightButtonLeft, FieldsTop + FieldStep, ButtonWidth, 5);
    error_ = new vgui2::Label(this, "MasterError", "");
    error_->SetBounds(Margin, ErrorTop, ContentWidth, FieldHeight);
    auto* ok = button("OK", "#ServerBrowser_OK", "OK",
        RightButtonLeft - ButtonWidth - ButtonGap, FooterTop, ButtonWidth, 6);
    ok->SetAsDefaultButton(1);
    button("Cancel", "#ServerBrowser_Cancel", "Cancel", RightButtonLeft, FooterTop, ButtonWidth, 7);
    list_->SetTabPosition(1);
    Populate();
    MoveToCenterOfScreen();
}
std::string BrowserSettingsDialog::Selection() const {
    if (!list_->GetSelectedItemsCount()) {
        return {};
    }
    return list_->GetItem(list_->GetSelectedItem(0))->GetString("id");
}
void BrowserSettingsDialog::Populate() {
    list_->RemoveAll();
    for (const auto& provider : draft_.Providers()) {
        auto* item = new KeyValues("masterserver");
        item->SetString("id", provider.id.c_str());
        item->SetString("Name", provider.name.c_str());
        item->SetInt("__cse_literal_Name", 1);
        std::string addresses;
        for (const auto& endpoint : provider.endpoints) {
            if (!addresses.empty()) {
                addresses += "; ";
            }
            addresses += endpoint;
        }
        item->SetString("Addresses", provider.id == "steam" ? "#CSEServerBrowser_SteamDirectory" : addresses.c_str());
        item->SetInt("__cse_literal_Addresses", provider.id == "steam" ? 0 : 1);
        const int row = list_->AddItem(item, 0, false, false);
        item->deleteThis();
        if (provider.id == draft_.selected) {
            list_->SetSingleSelectedItem(row);
        }
    }
    const auto selected = Selection();
    remove_->SetEnabled(!selected.empty() && selected != "steam" && selected != "varq");
}
void BrowserSettingsDialog::Retire() {
    if (!retired_) {
        retired_ = true;
        submit_ = {};
        Frame::OnClose();
        MarkForDeletion();
    }
}
void BrowserSettingsDialog::OnClose() {
    Retire();
}
void BrowserSettingsDialog::OnMessage(const KeyValues* message, vgui2::VPANEL sender) {
    if (std::strcmp(message->GetName(), "ItemSelected") == 0) {
        const auto selected = Selection();
        remove_->SetEnabled(!selected.empty() && selected != "steam" && selected != "varq");
    }
    Frame::OnMessage(message, sender);
}
void BrowserSettingsDialog::OnCommand(const char* command) {
    BrowserUse use(session_);
    if (retired_ || session_.closing || !session_.active) {
        Retire();
        return;
    }
    if (std::strcmp(command, "AddMasterserver") == 0) {
        if (!draft_.Add(EntryText(*name_, MaximumMasterNameBytes + 1),
                EntryText(*addresses_, MaximumMasterAddressBytes + 1))) {
            error_->SetText("#CSEServerBrowser_InvalidMasterserver");
            return;
        }
        draft_.selected = draft_.custom.back().id;
        name_->SetText("");
        addresses_->SetText("");
        error_->SetText("");
        Populate();
    } else if (std::strcmp(command, "RemoveMasterserver") == 0) {
        const auto selected = Selection();
        if (draft_.Remove(selected)) {
            Populate();
        }
    } else if (std::strcmp(command, "OK") == 0) {
        draft_.selected = Selection();
        if (!draft_.Valid()) {
            error_->SetText("#CSEServerBrowser_InvalidMasterserver");
            return;
        }
        auto submit = std::move(submit_);
        Retire();
        if (submit) {
            submit(draft_);
        }
    } else if (std::strcmp(command, "Cancel") == 0 || std::strcmp(command, "Close") == 0) {
        Retire();
    } else {
        Frame::OnCommand(command);
    }
}
}
