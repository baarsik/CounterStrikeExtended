#pragma once

#include <cstdint>
#include <string_view>

namespace cse::forms {

void RegisterServerForms();
void RunServerFormsFrame();
void EndServerFormsSession();
void SendFormAction(std::uint32_t token, unsigned control, std::string_view value);

} // namespace cse::forms
