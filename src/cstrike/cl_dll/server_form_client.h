#pragma once

#include "cse_server_forms.h"

namespace cse::forms {

void RegisterServerForms();
void RunServerFormsFrame();
void EndServerFormsSession();
void SendFormAction(std::uint32_t token, unsigned control, std::string_view value);
void RequestMenuOverride(StockMenu menu, int vguiMenu = 0);
void CancelVguiMenuOverride(int vguiMenu = 0);
void CancelTextMenuOverride();
void CancelBuyMenuOverride();
bool IsVguiMenuOverridden(int vguiMenu);

} // namespace cse::forms
