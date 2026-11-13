#include "model_fixture.inc"
#include "controls.h"
#include "../server_browser_ui/check.h"
#include "../server_browser_ui/geometry_fixture.h"
#include "../server_browser_ui/system_fixture.h"
#include "../../../src/GameUI/OptionsSubVoice.h"
#include "../../../src/GameUI/OptionsSubAudio.h"
#include "../../../src/GameUI/LabeledCommandComboBox.h"
#include "../../../src/GameUI/ModInfo.h"
#include "../../../src/GameUI/GameUI_Interface.h"
#include "../../../src/GameUI/CvarSlider.h"
#include "wrect.h"
#include "cl_dll.h"
#include "Button.h"
#include "CheckButton.h"
#include "Label.h"
#include "Slider.h"
#include "vgui/IVGui.h"
#include "vgui/ILocalize.h"
#include "vgui/IScheme.h"
#include "vgui/KeyValues.h"
#include "filesystem.h"
#include <cmath>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <map>

extern "C" void CseSteamModuleInitialize() {}

namespace {
constexpr int ScreenWidth = 640;
constexpr int ScreenHeight = 480;
constexpr int SettleFrames = 6;
constexpr float InitialMicrophoneVolume = 0.6f;
constexpr float InitialReceiveVolume = 0.8f;
constexpr float AppliedMicrophoneVolume = 0.4f;
constexpr int AppliedMicrophonePercent = 40;
constexpr float FloatTolerance = 0.001f;
float microphoneVolume = InitialMicrophoneVolume;
float receiveVolume = InitialReceiveVolume;
float boost = 0.0f;
float voiceEnabled = 1.0f;
int startCalls = 0;
int endCalls = 0;
int cvarWrites = 0;
std::map<std::string, float> audioValues = {{"volume", 1.0f}, {"suitvolume", 1.0f},
    {"mp3volume", 0.5f}, {"hisound", 1.0f}};
std::string audioCommand;

class TestAudio : public COptionsSubAudio {
public:
    explicit TestAudio(vgui2::Panel* parent) : COptionsSubAudio(parent) {}
    using COptionsSubAudio::OnApplyChanges;
};

int StartVoice() {
    ++startCalls;
    return 1;
}

void EndVoice() {
    ++endCalls;
}

void SetVoice(VoiceTweakControl control, float value) {
    if (control == MicrophoneVolume) {
        microphoneVolume = value;
    } else if (control == MicBoost) {
        boost = value;
    }
}

float GetVoice(VoiceTweakControl control) {
    return control == MicrophoneVolume ? microphoneVolume : boost;
}

int SpeakingVolume() {
    return 0;
}

float GetCvar(char* name) {
    const auto audio = audioValues.find(name);
    if (audio != audioValues.end()) {
        return audio->second;
    }
    return std::strcmp(name, "voice_scale") == 0 ? receiveVolume : voiceEnabled;
}

void SetCvar(char* name, float value) {
    ++cvarWrites;
    if (std::strcmp(name, "voice_scale") == 0) {
        receiveVolume = value;
    } else if (std::strcmp(name, "voice_modenable") == 0) {
        voiceEnabled = value;
    } else if (auto audio = audioValues.find(name); audio != audioValues.end()) {
        audio->second = value;
    } else {
        throw std::runtime_error("unexpected engine cvar write");
    }
}

int ClientCommand(char* command) {
    audioCommand = command;
    return 1;
}

void Settle(vgui2::Panel& root) {
    for (int index = 0; index < SettleFrames; ++index) {
        vgui2::ivgui()->RunFrame();
        vgui2::surface()->SolveTraverse(root.GetVPanel(), true);
    }
}

void CheckRetiredCredit(vgui2::Panel& page, const char* name) {
    auto* credit = dynamic_cast<vgui2::Label*>(page.FindChildByName(name, false));
    Require(credit != nullptr, "resource credit has a persistent retired control");
    Require(!credit->IsVisible(), "obsolete Miles credit must remain hidden");
    wchar_t text[256]{};
    credit->GetText(text, sizeof(text) / sizeof(text[0]));
    Require(text[0] == L'\0', "obsolete Miles credit text must be cleared");
}

void CheckCredit(COptionsSubVoice& page) {
    CheckRetiredCredit(page, "MilesVoiceLabel");
    for (const char* name : {"VoiceVolume", "VoiceReceive", "voice_modenable", "MicBoost", "TestMicrophone"}) {
        auto* control = page.FindChildByName(name, false);
        Require(control && control->IsVisible() && control->GetWide() > 0 && control->GetTall() > 0,
            "stock voice control missing or hidden");
    }
}
}

cl_enginefuncs_s gEngfuncs{};
cl_enginefuncs_s* engine = &gEngfuncs;

int main(int argc, char** argv) {
    Require(argc == 8, "usage: options_voice_tests vgui fs resources output language platform stock-valve");
    return RunCase([&] {
        const std::filesystem::path output = argv[4];
        std::filesystem::create_directories(output);
        void* image = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        void* fsImage = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
        Require(image && fsImage, "load owning VGUI and filesystem");
        auto guiFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(image, "CreateInterface"));
        auto fsFactory = reinterpret_cast<CreateInterfaceFn>(dlsym(fsImage, "CreateInterface"));
        auto* fs = static_cast<IFileSystem*>(fsFactory("VFileSystem009", nullptr));
        system_fixture::BorrowedSystem privateSystem(image,
            static_cast<vgui2::ISystem*>(guiFactory("VGUI_System009", nullptr)));
        fs->Mount();
        fs->AddSearchPathNoWrite(argv[6], "PLATFORM");
        for (const char* folder : {"platform", "valve", "cstrike"}) {
            fs->AddSearchPathNoWrite((std::string(argv[3]) + "/" + folder).c_str(), "GAME");
        }
        fs->AddSearchPathNoWrite(argv[7], "GAME");
        geometry_fixture::BaseUISurface geometry(
            static_cast<vgui2::ISurface*>(guiFactory("VGUI_Surface026", nullptr)));
        geometry_fixture::surface = &geometry;
        geometry.Resize(ScreenWidth, ScreenHeight);
        CreateInterfaceFn factories[] = {system_fixture::Factory, geometry_fixture::Factory, guiFactory, fsFactory};
        Require(vgui2::VGuiControls_Init("OPTIONS_VOICE", factories, 4), "initialize actual controls");
        vgui2::ivgui()->Start();
        vgui2::ivgui()->SetSleep(false);
        auto* root = new vgui2::Panel(nullptr, "VoiceRoot");
        root->SetBounds(0, 0, ScreenWidth, ScreenHeight);
        geometry.Root(root->GetVPanel());
        vgui2::surface()->SetLanguage(argv[5]);
        const auto tracker = vgui2::scheme()->LoadSchemeFromFile("resource/TrackerScheme.res", "Tracker");
        Require(tracker != 0, "load actual Tracker scheme");
        root->SetScheme(tracker);
        for (const char* file : {"resource/valve_%language%.txt", "resource/gameui_%language%.txt"}) {
            Require(vgui2::localize()->AddFile(fs, file), "load stock UI translations");
        }
        auto* resource = new KeyValues("Resource\\OptionsSubVoice.res");
        Require(resource->LoadFromFile(fs, "Resource\\OptionsSubVoice.res", "GAME"), "load installed voice resource");
        auto* creditSettings = resource->FindKey("MilesVoiceLabel");
        Require(creditSettings && creditSettings->GetInt("visible") == 1 &&
            std::strcmp(creditSettings->GetString("labelText"), "#GameUI_Miles_Voice") == 0,
            "fixture must use official resource with visible obsolete credit");
        IVoiceTweak voice = {StartVoice, EndVoice, SetVoice, GetVoice, SpeakingVolume};
        gEngfuncs.pVoiceTweak = &voice;
        gEngfuncs.pfnGetCvarFloat = GetCvar;
        gEngfuncs.Cvar_SetValue = SetCvar;
        gEngfuncs.pfnClientCmd = ClientCommand;

        auto* audioResource = new KeyValues("Resource\\OptionsSubAudio.res");
        Require(audioResource->LoadFromFile(fs, "Resource\\OptionsSubAudio.res", "GAME"),
            "load installed audio resource");
        auto* audioCreditSettings = audioResource->FindKey("MilesAudioLabel");
        Require(audioCreditSettings && audioCreditSettings->GetInt("visible") == 1 &&
            std::strcmp(audioCreditSettings->GetString("labelText"), "#GameUI_Miles_Audio") == 0,
            "fixture must use official audio resource with visible obsolete credit");
        auto* audio = new TestAudio(root);
        audio->SetBounds(0, 0, ScreenWidth, ScreenHeight);
        Settle(*root);
        CheckRetiredCredit(*audio, "MilesAudioLabel");
        auto* retiredAudioCredit = audio->FindChildByName("MilesAudioLabel", false);
        for (int reload = 0; reload < 2; ++reload) {
            audio->LoadControlSettings("Resource\\OptionsSubAudio.res", "GAME");
            audio->InvalidateLayout(true, true);
            Settle(*root);
            CheckRetiredCredit(*audio, "MilesAudioLabel");
            Require(audio->FindChildByName("MilesAudioLabel", false) == retiredAudioCredit,
                "audio resource reload must retain the retired credit control");
        }
        constexpr float AppliedAudioVolume = 0.7f;
        for (const char* name : {"SFX Slider", "Suit Slider", "MP3 Volume"}) {
            auto* slider = dynamic_cast<CCvarSlider*>(audio->FindChildByName(name, false));
            Require(slider && slider->IsVisible() && slider->IsEnabled() && slider->GetWide() > 0,
                "existing audio slider remains visible and enabled");
            slider->SetSliderValue(AppliedAudioVolume);
        }
        auto* quality = dynamic_cast<CLabeledCommandComboBox*>(audio->FindChildByName("Sound Quality", false));
        Require(quality && quality->IsVisible() && quality->IsEnabled(), "existing sound quality remains usable");
        quality->ActivateItem(1);
        audio->OnApplyChanges();
        for (const char* name : {"volume", "suitvolume", "mp3volume"}) {
            Require(std::fabs(audioValues.at(name) - AppliedAudioVolume) < FloatTolerance,
                "existing audio sliders apply through actual owner");
        }
        Require(audioCommand == "hisound 0", "existing sound quality applies through actual owner");
        CheckRetiredCredit(*audio, "MilesAudioLabel");
        delete audio;
        audioResource->deleteThis();
        ModInfo()->FreeModInfo();
        auto* page = new COptionsSubVoice(root);
        page->SetBounds(0, 0, ScreenWidth, ScreenHeight);
        Settle(*root);
        CheckCredit(*page);
        auto* retiredCredit = page->FindChildByName("MilesVoiceLabel", false);
        for (int reload = 0; reload < 2; ++reload) {
            page->LoadControlSettings("Resource\\OptionsSubVoice.res", "GAME");
            page->InvalidateLayout(true, true);
            Settle(*root);
            CheckCredit(*page);
            Require(page->FindChildByName("MilesVoiceLabel", false) == retiredCredit,
                "resource reload must retain the retired credit control");
        }
        auto* microphone = dynamic_cast<vgui2::Slider*>(page->FindChildByName("VoiceVolume", false));
        auto* micBoost = dynamic_cast<vgui2::CheckButton*>(page->FindChildByName("MicBoost", false));
        Require(microphone && micBoost && microphone->IsEnabled() && micBoost->IsEnabled(),
            "voice API enables existing controls");
        microphone->SetValue(AppliedMicrophonePercent, true);
        micBoost->SetSelected(true);
        page->OnApplyChanges();
        Require(std::fabs(microphoneVolume - AppliedMicrophoneVolume) < FloatTolerance && boost == 1.0f,
            "existing microphone volume and boost apply through actual owner");
        page->OnCommand("TestMicrophone");
        Require(startCalls == 1 && !microphone->IsEnabled(), "actual voice test starts and disables controls");
        page->OnCommand("TestMicrophone");
        Require(endCalls == 1 && microphone->IsEnabled(), "actual voice test ends and restores controls");
        CheckCredit(*page);
        delete page;
        gEngfuncs.pVoiceTweak = nullptr;
        auto* unsupported = new COptionsSubVoice(root);
        Settle(*root);
        CheckCredit(*unsupported);
        Require(!unsupported->FindChildByName("VoiceVolume", false)->IsEnabled() &&
            !unsupported->FindChildByName("TestMicrophone", false)->IsEnabled(),
            "missing voice API still disables existing controls");
        std::ofstream(output / "voice-results.txt") << "PASS: official Audio/Voice credits retired; resource/scheme reload; "
            "existing controls; apply; voice test; missing voice API. Controlled cvar writes: " << cvarWrites << '\n';
        resource->deleteThis();
        vgui2::surface()->SetEmbeddedPanel(0);
        delete root;
        vgui2::ivgui()->Shutdown();
        vgui2::scheme()->Shutdown(true);
        vgui2::surface()->Shutdown();
        vgui2::VGuiControls_Shutdown();
        fs->Unmount();
        privateSystem.Restore();
        std::puts("PASS: official Audio/Voice credit retirement and existing sound controls");
    });
}
