// The real loader inserts this inert image; no API interposition or constructor is needed.
extern "C" __attribute__((visibility("default"))) int CseOwnerBenignInsertion() {
    return 0;
}
