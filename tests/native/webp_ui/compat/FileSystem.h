#pragma once
#include <cstdio>
namespace vgui2 {
class TestFileSystem {
public:
    void Write(const void* data, int bytes, void* file) {
        std::fwrite(data, 1, static_cast<std::size_t>(bytes), static_cast<FILE*>(file));
    }
};
}
