#pragma once
// A main-thread snapshot of the existing engine-owned provider. Reading it never initializes a provider.
struct AdminProviderStatus {
    bool available = false;
    bool connected = false;
    bool secureRequested = false;
    bool secure = false;
    bool restartRequested = false;
    int failure = 0;
};
AdminProviderStatus Steam_QueryAdminStatus();
