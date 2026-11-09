#include "TextEntryClipboard.h"
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

using cse_textentry_clipboard::Buffer;
using cse_textentry_clipboard::PasteFrame;
static std::vector<wchar_t> source;
static std::vector<int> capacities;
static int allocations = 0;
static int releases = 0;
static int failAt = -1;
static int reports = INT_MIN;
static bool throwsOnRead = false;

static void Check(bool condition) {
    if (!condition) {
        throw std::runtime_error("private helper contract");
    }
}

static void* Allocate(std::size_t bytes) {
    ++allocations;
    return allocations == failAt ? nullptr : std::malloc(bytes);
}

static void Release(void* memory) {
    ++releases;
    std::free(memory);
}

static int Read(wchar_t* destination, int bytes) {
    const int capacity = bytes / static_cast<int>(sizeof(wchar_t));
    capacities.push_back(capacity);
    if (throwsOnRead) {
        throw std::runtime_error("controlled reader failure");
    }
    const int copied = std::min(capacity, static_cast<int>(source.size()));
    std::copy_n(source.data(), copied, destination);
    return reports == INT_MIN ? copied : reports;
}

int main() {
    try {
        {
            Buffer defaultStorage;
            Check(defaultStorage.CopyExpanded(L"native", 6) && defaultStorage.Length() == 6);
        }
        {
            Buffer buffer({Allocate, Release});
            Check(buffer.CopyExpanded(L"A\nB\r\nC", 6));
            Check(std::vector<wchar_t>(buffer.Data(), buffer.Data() + buffer.Length())
                  == std::vector<wchar_t>{L'A', L'\r', L'\n', L'B', L'\r', L'\r', L'\n', L'C'});
            Check(buffer.Data()[buffer.Length()] == 0);
            const wchar_t* retained = buffer.Data();
            Check(!buffer.CopyExpanded(nullptr, 1));
            Check(!buffer.CopyExpanded(L"", -1));
            Check(!buffer.CopyExpanded(reinterpret_cast<const wchar_t*>(1), INT_MAX));
            Check(buffer.Data() == retained);
            failAt = allocations + 1;
            Check(!buffer.CopyExpanded(L"x", 1));
            Check(buffer.Data() == retained);
            failAt = -1;
            source = {L'A', L'B', L'C', 0};
            Check(buffer.Read(1, true, Read));
            Check(capacities == std::vector<int>{1, 2, 4});
            Check(buffer.Length() == 3 && buffer.Data()[2] == L'C');
            capacities.clear();
            Check(buffer.Read(2, false, Read));
            Check(buffer.Length() == 1 && buffer.Data()[0] == L'A');
            retained = buffer.Data();
            source = {L'X', L'Y'};
            Check(!buffer.Read(8, true, Read));
            Check(buffer.Data() == retained);
            source = {L'Z', 0};
            reports = 0;
            Check(!buffer.Read(8, true, Read));
            Check(buffer.Data() == retained);
            reports = INT_MAX;
            Check(buffer.Read(2, true, Read) && buffer.Length() == 1);
            reports = INT_MIN;
            source = {L'X', L'Y', L'Z', 0};
            failAt = allocations + 2;
            retained = buffer.Data();
            Check(!buffer.Read(1, true, Read));
            Check(buffer.Data() == retained);
            failAt = -1;
            Check(!buffer.Read(0, true, Read));
            Check(!buffer.Read(INT_MAX, true, Read));
            Check(!buffer.Read(1, true, nullptr));
            throwsOnRead = true;
            bool caught = false;
            try {
                buffer.Read(1, true, Read);
            } catch (const std::runtime_error&) {
                caught = true;
            }
            Check(caught && buffer.Data() == retained);
            throwsOnRead = false;
            Check(buffer.CopyExpanded(nullptr, 0) && buffer.Length() == 0 && buffer.Data()[0] == 0);
        }
        // Two intentional failed allocations were never owned/released.
        Check(allocations == releases + 2);
        int panel = 0;
        {
            PasteFrame outer(&panel, 5);
            PasteFrame equal(&panel, 5);
            PasteFrame greater(&panel, 6);
            Check(outer.Entered() && !equal.Entered() && !greater.Entered());
            {
                PasteFrame smaller(&panel, 4);
                Check(smaller.Entered());
                PasteFrame noProgress(&panel, 4);
                Check(!noProgress.Entered());
            }
            bool workerEntered = false;
            std::thread worker([&] {
                PasteFrame otherThread(&panel, 5);
                workerEntered = otherThread.Entered();
            });
            worker.join();
            Check(workerEntered);
        }
        PasteFrame later(&panel, 5);
        Check(later.Entered());
        std::vector<int> panels(2048);
        std::vector<std::unique_ptr<PasteFrame>> frames;
        for (int& identity : panels) {
            frames.emplace_back(new PasteFrame(&identity, 1));
            Check(frames.back()->Entered());
        }
        while (!frames.empty()) {
            frames.pop_back();
        }
        PasteFrame invalid(nullptr, 0);
        Check(!invalid.Entered());
        std::cout << "PASS private helper storage/growth/failure/TLS controls\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << "\n";
        return 1;
    }
}
