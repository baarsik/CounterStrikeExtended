#include "html_legacy_interfaces.h"
#include "../../../GoldSrc-SDK/engine/html/ichromehtmlwrapper.h"
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <type_traits>

static_assert(std::is_same_v<decltype(&IHTMLSerializer::KeyChar), void (IHTMLSerializer::*)(int)>);
static_assert(std::is_same_v<decltype(&IHTMLSerializer::GetBrowserHandle), std::uintptr_t (IHTMLSerializer::*)()>);
static_assert(!std::has_virtual_destructor_v<IHTMLSerializer>);
static_assert(sizeof(IHTMLSerializer) == sizeof(void*));
static_assert(std::has_virtual_destructor_v<IHTMLChromeController>);
#if UINTPTR_MAX > UINT32_MAX
static_assert(std::is_abstract_v<IHTMLChromeController>);
#endif
constexpr int BrowserIndex = 7;
constexpr int ExpectedCalls = 7;
constexpr int CompleteVirtualCalls = 28;
constexpr int HighSurrogate = 0xd83d;
constexpr int LowSurrogate = 0xde00;

class Serializer final : public IHTMLSerializer {
public:
    int calls = 0;
    int lastCharacter = 0;
    std::uintptr_t identity = 0;
    IHTMLResponses* GetResponseTarget() override {
        ++calls;
        return reinterpret_cast<IHTMLResponses*>(identity);
    }
    int BrowserGetIndex() override {
        ++calls;
        return BrowserIndex;
    }
    void BrowserPosition(int, int) override {
        ++calls;
    }
    void PostURL(const char*, const char*) override {
        ++calls;
    }
    void BrowserErrorStrings(const char*, const char*, const char*, const char*, const char*, const char*,
        const char*) override {
        ++calls;
    }
    void StopLoad() override {
        ++calls;
    }
    void Reload() override {
        ++calls;
    }
    void GoBack() override {
        ++calls;
    }
    void GoForward() override {
        ++calls;
    }
    void MouseDown(int) override {
        ++calls;
    }
    void MouseUp(int) override {
        ++calls;
    }
    void MouseWheel(int) override {
        ++calls;
    }
    void MouseMove(int, int) override {
        ++calls;
    }
    void MouseDoubleClick(int) override {
        ++calls;
    }
    void BrowserSize(int, int) override {
        ++calls;
    }
    void KeyUp(int, int) override {
        ++calls;
    }
    void KeyDown(int, int) override {
        ++calls;
    }
    void KeyChar(int character) override {
        ++calls;
        lastCharacter = character;
    }
    void RunJavaScript(const char*) override {
        ++calls;
    }
    void SetHorizontalScroll(int) override {
        ++calls;
    }
    void SetVerticalScroll(int) override {
        ++calls;
    }
    void SetFocus(bool) override {
        ++calls;
    }
    void AddHeader(const char*, const char*) override {
        ++calls;
    }
    void NeedsPaintResponse(int) override {
        ++calls;
    }
    void StartRequestResponse(bool) override {
        ++calls;
    }
    void RequestBrowserSizes() override {
        ++calls;
    }
    void JSDialogResponse(int) override {
        ++calls;
    }
    void GetLinkAtPosition(int, int) override {
        ++calls;
    }
};

int main() {
    Serializer serializer;
    serializer.identity = reinterpret_cast<std::uintptr_t>(&serializer);
    IHTMLSerializer& contract = serializer;
    if (contract.GetBrowserHandle() != serializer.identity || contract.GetBrowserIndex() != BrowserIndex) {
        return 1;
    }
    if (sizeof(void*) == 8 && serializer.identity <= UINT32_MAX) {
        return 1;
    }
    contract.ExecuteJavascript("void 0");
    contract.FullRepaint();
    contract.LinkAtPosition(1, 2);
    contract.KeyChar(HighSurrogate);
    contract.KeyChar(LowSurrogate);
    if (serializer.calls != ExpectedCalls || serializer.lastCharacter != LowSurrogate) {
        return 1;
    }
    serializer.calls = 0;
    contract.GetResponseTarget();
    contract.BrowserGetIndex();
    contract.BrowserPosition(1, 2);
    contract.PostURL("https://fixture.invalid/", "field=value");
    contract.BrowserErrorStrings("title", "connection", "retry", "unknown", "cache", "url", "detail");
    contract.StopLoad();
    contract.Reload();
    contract.GoBack();
    contract.GoForward();
    contract.MouseDown(1);
    contract.MouseUp(1);
    contract.MouseWheel(1);
    contract.MouseMove(1, 2);
    contract.MouseDoubleClick(1);
    contract.BrowserSize(1, 2);
    contract.KeyUp(1, 2);
    contract.KeyDown(1, 2);
    contract.KeyChar(LowSurrogate);
    contract.RunJavaScript("void 0");
    contract.SetHorizontalScroll(1);
    contract.SetVerticalScroll(1);
    contract.SetFocus(true);
    contract.AddHeader("X-Fixture", "value");
    contract.NeedsPaintResponse(1);
    contract.StartRequestResponse(true);
    contract.RequestBrowserSizes();
    contract.JSDialogResponse(1);
    contract.GetLinkAtPosition(1, 2);
    if (serializer.calls != CompleteVirtualCalls) {
        return 1;
    }
    constexpr int SupplementaryCharacter = 0x1f600;
    contract.KeyChar(SupplementaryCharacter);
    if (serializer.lastCharacter != SupplementaryCharacter) {
        return 1;
    }
    std::puts("native_html_contract_passed=1");
}
