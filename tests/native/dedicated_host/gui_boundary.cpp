#include "dedicated/native/NativeGui.h"
#include <cstdlib>
namespace cse::dedicated {
NativeGui::~NativeGui() {}
bool NativeGui::Open(DedicatedModuleSet&, std::string&, std::uint64_t, const std::string&) { std::abort(); }
bool NativeGui::Pump() { std::abort(); }
bool NativeGui::ReadyToStart() const { std::abort(); }
bool NativeGui::Started(std::string&) { std::abort(); }
const char* NativeGui::Commands() const { std::abort(); }
void NativeGui::Stop() noexcept { std::abort(); }
void NativeGui::Close() noexcept { std::abort(); }
void NativeGui::Print(char*) { std::abort(); }
}
