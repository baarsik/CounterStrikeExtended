#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include "legacy_text_encoding.h"

#define MAX_BUTTON_SIZE 32
struct client_textmessage_t {
    const char* pMessage;
};
std::map<std::string, std::string> titles;
client_textmessage_t* TextMessageGet(const char* name) {
    const auto found = titles.find(name);
    if (found == titles.end()) {
        return nullptr;
    }
    static client_textmessage_t result;
    result.pMessage = found->second.c_str();
    return &result;
}
class CHudTextMessage {
public:
    static char* LocaliseTextString(const char*, char*, int);
};
#include "localize.inc"
struct MenuResult {
    std::string label;
    std::string errorLabel;
};
#include "menu.inc"
class Button {
public:
    std::string display;
    bool sizedText = false;
    void setText(int, const char* text) {
        display = text;
        sizedText = true;
    }
    void setText(const char* text) {
        display = text;
    }
};
class CommandButton : public Button {
public:
#include "button-field.inc"
    char m_cBoundKey = 0;
    void RecalculateText();
    void setText(const char*);
    void setBoundKey(char);
};
#include "button.inc"

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
void Bounded(const char* input, int capacity, const std::string& expected) {
    constexpr std::size_t GuardBytes = 8;
    constexpr unsigned char Canary = 0xa5;
    std::vector<unsigned char> memory(GuardBytes * 2 + static_cast<std::size_t>(capacity), Canary);
    char* destination = reinterpret_cast<char*>(memory.data() + GuardBytes);
    Require(CHudTextMessage::LocaliseTextString(input, destination, capacity) == destination, "return destination");
    for (std::size_t index = 0; index < GuardBytes; ++index) {
        Require(memory[index] == Canary && memory[GuardBytes + static_cast<std::size_t>(capacity) + index] == Canary,
            "localization must preserve short-destination canaries");
    }
    if (capacity > 0) {
        Require(std::strcmp(destination, expected.c_str()) == 0, "bounded prefix preserves complete UTF8 scalars");
    }
}
int main(int argc, char** argv) {
    Require(argc == 3, "title file and scenario arguments");
    std::ifstream input(argv[1], std::ios::binary);
    std::string name;
    std::string body;
    while (std::getline(input, name, '\0')) {
        Require(static_cast<bool>(std::getline(input, body, '\0')), "complete title body");
        titles[name] = body;
    }
    Require(!titles.empty(), "actual Russian/Polish menu title records");
    const std::string scenario = argv[2];
    if (scenario == "menu" || scenario == "all") {
        for (const auto& record : titles) {
            const auto result = ActualMenuLabel(("#" + record.first).c_str());
            Require(result.label == record.second && result.errorLabel == record.second,
                "actual menu scratch and last-error label preserve full localized title");
            CommandButton button;
            button.setText(result.label.c_str());
            Require(button.sizedText, "button uses existing length-aware literal text overload");
            Require(button.display == "     " + record.second, "actual unbound button preserves full localized title");
            button.setBoundKey('4');
            Require(button.display == "  4  " + record.second, "actual bound button preserves full localized title");
            button.setBoundKey(static_cast<char>(255));
            Require(button.display == record.second, "actual keyless button preserves full localized title");
        }
    }
    if (scenario == "bounds" || scenario == "all") {
        titles["SHORT_RU"] = u8"Привет";
        titles["SHORT_PL"] = u8"Zażółć";
        const std::vector<std::string> polish{"", "Z", "Za", "Za", u8"Zaż", u8"Zaż", u8"Zażó", u8"Zażó",
            u8"Zażół", u8"Zażół", u8"Zażółć"};
        for (int capacity = 1; capacity <= 14; ++capacity) {
            const auto bytes = static_cast<std::size_t>((capacity - 1) / 2) * 2;
            Bounded("#SHORT_RU", capacity, titles["SHORT_RU"].substr(0, (std::min)(bytes, std::size_t{12})));
        }
        for (std::size_t capacity = 1; capacity <= polish.size(); ++capacity) {
            Bounded("#SHORT_PL", static_cast<int>(capacity), polish[capacity - 1]);
        }
        Bounded("A#SHORT_RU!", 64, std::string("A") + u8"Привет!");
        Bounded("#SHORT_RU#SHORT_PL", 64, std::string(u8"Привет") + u8"Zażółć");
        titles["NESTED"] = "#SHORT_PL";
        Bounded("#NESTED", 64, "#SHORT_PL");
        char inPlace[32] = "#SHORT_RU";
        CHudTextMessage::LocaliseTextString(inPlace, inPlace, sizeof(inPlace));
        Require(std::strcmp(inPlace, u8"Привет") == 0, "in-place localization preserves source before copying");
        CommandButton longButton;
        const std::string longLabel(5000, 'x');
        longButton.setText(longLabel.c_str());
        Require(longButton.display == "     " + longLabel && longButton.sizedText,
            "private button storage/display has no hidden larger fixed-buffer cutoff");
        Bounded("#MISSING!", 64, "#MISSING!");
        Bounded(("#" + std::string(300, 'X')).c_str(), 8, "#XXXXXX");
        Bounded("plain", 6, "plain");
        Bounded("plain", 5, "plai");
        Bounded("plain", 1, "");
        Bounded("plain", 0, "");
        Bounded(nullptr, 4, "");
        Bounded("", 4, "");
        Bounded("\xc3", 2, std::string("\xc3", 1));
        Bounded("\x93X", 2, std::string("\x93", 1));
        Bounded(u8"😀", 4, "");
        Bounded(u8"😀", 5, u8"😀");
        char untouched = 'x';
        Require(CHudTextMessage::LocaliseTextString("text", &untouched, -1) == &untouched && untouched == 'x',
            "negative capacity does not write");
        Require(CHudTextMessage::LocaliseTextString("text", nullptr, 4) == nullptr, "null destination is safe");
    }
    std::printf("PASS: %zu actual menu labels and bounded localization\n", titles.size());
}
