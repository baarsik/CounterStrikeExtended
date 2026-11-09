extern "C" int RootPing();

// This real dependent dylib deliberately has NO CreateInterface export. dlsym
// follows its dependency and can return root's factory for this foreign handle.
extern "C" __attribute__((visibility("default"))) int ProxyPing() {
    return RootPing();
}
