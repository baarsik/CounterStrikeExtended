#pragma once

#include "include/cef_app.h"

int RunCefProbe(const CefMainArgs& arguments, CefRefPtr<CefCommandLine> commandLine, void* sandbox);
void CloseCefProbe();
