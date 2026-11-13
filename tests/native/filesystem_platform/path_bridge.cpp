#include "../../../src/filesystem/pathmatch.cpp"

extern "C" int CseFixtureDescend(char* path, unsigned int start, bool allowMismatch, unsigned int level) {
    s_bShowDiag = true;
    return Descend(path, start, allowMismatch, level);
}
