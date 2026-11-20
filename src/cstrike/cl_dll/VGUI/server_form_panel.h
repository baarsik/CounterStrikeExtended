#pragma once

#include <cstdint>
#include <string_view>

namespace cse::forms {

struct Form;
struct Image;

using ImageLookupFn = const Image* (*)(std::string_view);

void ShowFormPanel(const Form& form, std::uint32_t token, ImageLookupFn imageLookup);
void CloseFormPanel();
void SetFormPanelClientUIActive(bool active);

}
