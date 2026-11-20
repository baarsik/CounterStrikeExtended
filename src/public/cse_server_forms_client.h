#pragma once

#include "tier1/interface.h"

namespace cse::forms {

inline constexpr char ClientInputVersion[] = "CSEServerFormInput001";

class ClientInput : public IBaseInterface {
public:
    virtual bool ConsumeKeyPress(int key, bool repeat, bool shortcutModifierDown) = 0;
    virtual void EndServerFormsSession() = 0;
};

}
