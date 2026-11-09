#include "VGui1LegacyRaster.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace {
using namespace cse_vgui1::legacy;
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED GDI BGR transfer replay: %s\n", message);
        std::abort();
    }
}

std::vector<unsigned char> Read(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    Require(input.good(), "owned replay input exists");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
}

int main(int argc, char** argv) {
    Require(argc == 2, "frozen Windows authored reference directory");
    constexpr std::size_t kPixelBytes = 4;
    std::size_t total = 0;
    for (const char* order : {"ascending", "descending", "sequence"}) {
        const std::string prefix = std::string(argv[1]) + "/" + order;
        std::ifstream meta(prefix + "-metrics.tsv");
        std::string line;
        Require(static_cast<bool>(std::getline(meta, line)), "owned metadata first row");
        std::istringstream values(line);
        std::vector<int> metrics;
        int value = 0;
        while (values >> value) {
            metrics.push_back(value);
        }
        Require(metrics.size() == 14 && metrics[9] > 0 && metrics[10] > 0
            && metrics[11] > 0 && metrics[12] > 0 && metrics[13] > 0, "bounded replay metadata fields");
        const auto source = Read(prefix + "-bgr.bin");
        const auto reference = Read(prefix + "-rgba.bin");
        const std::size_t sourceFrame = static_cast<std::size_t>(metrics[11]) * metrics[12] * kPixelBytes;
        const std::size_t outputFrame = static_cast<std::size_t>(metrics[9]) * metrics[10] * kPixelBytes;
        Require(source.size() == sourceFrame * static_cast<std::size_t>(metrics[13])
            && reference.size() == outputFrame * static_cast<std::size_t>(metrics[13]),
            "exact frame counts and bytes");
        std::ifstream abc(prefix + "-abc.tsv");
        std::vector<unsigned char> output(outputFrame);
        for (int ordinal = 0; ordinal < metrics[13]; ++ordinal) {
            Require(static_cast<bool>(std::getline(abc, line)), "ABC record exists");
            std::istringstream record(line);
            int row[10]{};
            for (int& field : row) {
                Require(static_cast<bool>(record >> field), "complete ABC record");
            }
            Require(row[0] == ordinal && row[1] >= 0 && row[1] <= 255 && row[5] >= 0, "ABC replay range");
            std::fill(output.begin(), output.end(), static_cast<unsigned char>(0x5a));
            const BgrView view{source.data() + static_cast<std::size_t>(ordinal) * sourceFrame,
                sourceFrame, metrics[11], metrics[12], static_cast<std::ptrdiff_t>(metrics[11]) * 4};
            const RgbaCanvas canvas{output.data(), output.size(), metrics[9], metrics[10]};
            Require(TransferLegacyBgr(view, (std::min)(row[5], metrics[11]), (std::min)(metrics[0], metrics[12]),
                3, 5, canvas, row[1] == '\t') == Result::Ok, "actual transfer accepts captured valid source");
            for (std::size_t offset = 0; offset < output.size(); ++offset) {
                const auto expected = reference[static_cast<std::size_t>(ordinal) * outputFrame + offset];
                if (output[offset] != expected) {
                    std::fprintf(stderr, "%s ordinal=%d byte=%d offset=%zu GDI=%u replay=%u\n",
                        order, ordinal, row[1], offset, static_cast<unsigned int>(expected),
                        static_cast<unsigned int>(output[offset]));
                    return 1;
                }
            }
            total += output.size();
        }
    }
    std::printf("PASS exact GDI BGR/weighted-alpha/canvas/pen replay: %zu bytes\n", total);
}
