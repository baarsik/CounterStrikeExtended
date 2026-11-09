#include "keyvalues_directories.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
struct Request {
    std::string path;
    const char* pathID;
};

struct FileSystem {
    void CreateDirHierarchy(const char* path, const char* pathID) {
        requests.push_back({path, pathID});
    }
    std::vector<Request> requests;
};

void Check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(EXIT_FAILURE);
    }
}
}

int main() {
    FileSystem filesystem;
    const char pathID[] = "GAMECONFIG";
    for (const char* path : {"cfg/nested/value.vdf", "cfg\\nested\\value.vdf", "cfg\\nested/value.vdf"}) {
        const std::string before = path;
        cse::PrepareKeyValuesParentDirectory(&filesystem, path, pathID);
        Check(filesystem.requests.back().path == "cfg/nested" && filesystem.requests.back().pathID == pathID,
            "Parent path or path-ID provenance changed");
        Check(before == path, "Resource name was mutated");
    }
    const std::size_t beforeEmpty = filesystem.requests.size();
    for (const char* path : {"value.vdf", "", "/value.vdf", "\\value.vdf"}) {
        cse::PrepareKeyValuesParentDirectory(&filesystem, path, nullptr);
    }
    cse::PrepareKeyValuesParentDirectory(&filesystem, nullptr, pathID);
    cse::PrepareKeyValuesParentDirectory(static_cast<FileSystem*>(nullptr), "cfg/value.vdf", pathID);
    Check(filesystem.requests.size() == beforeEmpty, "Absent parent made a directory request");
    const std::string lastValid(cse::KeyValuesParentPathBytes - 1, 'a');
    cse::PrepareKeyValuesParentDirectory(&filesystem, (lastValid + "/file").c_str(), nullptr);
    Check(filesystem.requests.back().path == lastValid && filesystem.requests.back().pathID == nullptr,
        "Largest terminated parent or null path-ID changed");
    const std::size_t beforeOverflow = filesystem.requests.size();
    cse::PrepareKeyValuesParentDirectory(&filesystem, (lastValid + "a/file").c_str(), pathID);
    Check(filesystem.requests.size() == beforeOverflow, "Oversize parent caused a partial directory write");
    cse::PrepareKeyValuesParentDirectory(&filesystem, "../cfg/value.vdf", pathID);
    Check(filesystem.requests.back().path == "../cfg", "The filesystem's relative-path policy was replaced");
    std::puts("PASS native KeyValues parents, separators, path-ID identity, bounds and unchanged resource input");
}
