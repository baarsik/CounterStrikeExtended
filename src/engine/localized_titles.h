#ifndef CSE_ENGINE_LOCALIZED_TITLES_H
#define CSE_ENGINE_LOCALIZED_TITLES_H

#include "../common/Sequence.h"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace cse {
namespace localization {

struct TitleRecord {
    client_textmessage_t value{};
    std::string name;
    std::string text;
};

inline std::string TitleKey(const std::string& name) {
    std::string key = name;
    constexpr char LowercaseOffset = 'a' - 'A';
    for (char& character : key) {
        if (character >= 'A' && character <= 'Z') {
            character += LowercaseOffset;
        }
    }

    return key;
}

inline std::string TrimTitleLine(const std::string& line) {
    const auto first = line.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }

    return line.substr(first, line.find_last_not_of(" \t\r\n") - first + 1);
}

inline bool ParseTitleRecords(const std::string& source, std::vector<TitleRecord>& records) {
    if (source.find('\0') != std::string::npos) {
        return false;
    }

    constexpr std::size_t MaximumMessages = 1000;
    constexpr std::size_t MaximumNameBytes = 16384;
    client_textmessage_t parameters{};
    std::string name;
    std::string text;
    bool inText = false;
    std::size_t offset = 0;
    std::size_t nameBytes = 0;
    while (offset < source.size()) {
        const auto newline = source.find('\n', offset);
        const auto end = newline == std::string::npos ? source.size() : newline + 1;
        const std::string line = source.substr(offset, end - offset);
        offset = end;
        const std::string trimmed = TrimTitleLine(line);
        if (inText) {
            if (trimmed == "}") {
                if (!text.empty() && text.back() == '\n') {
                    text.pop_back();
                    // Match the cold parser's retained CR for both LF and CRLF files.
                    if (text.empty() || text.back() != '\r') {
                        text += '\r';
                    }
                }
                if (records.size() >= MaximumMessages || nameBytes + name.size() + 1 > MaximumNameBytes) {
                    return false;
                }
                records.push_back({parameters, name, text});
                nameBytes += name.size() + 1;
                name.clear();
                text.clear();
                inText = false;
            } else if (trimmed == "{") {
                return false;
            } else {
                text += line;
            }
            continue;
        }
        if (trimmed.empty() || trimmed.compare(0, 2, "//") == 0) {
            continue;
        }
        if (trimmed[0] == '$') {
            std::istringstream fields(trimmed);
            std::string directive;
            fields >> directive;
            constexpr int MaximumDirectiveValues = 3;
            float values[MaximumDirectiveValues]{};
            int count = 1;
            if (directive == "$position") {
                count = 2;
            } else if (directive == "$color" || directive == "$color2") {
                count = MaximumDirectiveValues;
            } else if (directive != "$effect" && directive != "$fadein" && directive != "$fadeout"
                && directive != "$holdtime" && directive != "$fxtime") {
                continue;
            }
            for (int index = 0; index < count; ++index) {
                if (!(fields >> values[index]) || !std::isfinite(values[index])) {
                    return false;
                }
            }
            if (directive == "$position") {
                parameters.x = values[0];
                parameters.y = values[1];
            } else if (directive == "$effect") {
                constexpr int MaximumEffect = 2;
                if (values[0] < 0 || values[0] > MaximumEffect) {
                    return false;
                }
                parameters.effect = static_cast<int>(values[0]);
            } else if (directive == "$color" || directive == "$color2") {
                constexpr float MaximumColor = 255.0f;
                for (float value : values) {
                    if (value < 0 || value > MaximumColor) {
                        return false;
                    }
                }
                if (directive == "$color") {
                    parameters.r1 = static_cast<byte>(values[0]);
                    parameters.g1 = static_cast<byte>(values[1]);
                    parameters.b1 = static_cast<byte>(values[2]);
                } else {
                    parameters.r2 = static_cast<byte>(values[0]);
                    parameters.g2 = static_cast<byte>(values[1]);
                    parameters.b2 = static_cast<byte>(values[2]);
                }
            } else if (directive == "$fadein") {
                parameters.fadein = values[0];
            } else if (directive == "$fadeout") {
                parameters.fadeout = values[0];
            } else if (directive == "$holdtime") {
                parameters.holdtime = values[0];
            } else if (directive == "$fxtime") {
                parameters.fxtime = values[0];
            }
        } else if (trimmed == "{") {
            if (name.empty()) {
                return false;
            }
            inText = true;
        } else if (trimmed == "}") {
            return false;
        } else {
            // Retained CS catalogues contain a heading/list before the actual
            // name; the legacy parser keeps the last line before an opening brace.
            name = trimmed;
        }
    }

    return !inText && name.empty();
}

class TitleSnapshot {
public:
    explicit TitleSnapshot(std::vector<TitleRecord> records) : records_(std::move(records)) {
        values_.reserve(records_.size());
        for (const auto& record : records_) {
            client_textmessage_t value = record.value;
            value.pName = record.name.c_str();
            value.pMessage = record.text.c_str();
            values_.push_back(value);
        }
    }

    client_textmessage_t* Data() {
        return values_.data();
    }

    int Count() const {
        return static_cast<int>(values_.size());
    }

    bool Contains(const client_textmessage_t* pointer) const {
        const auto address = reinterpret_cast<std::uintptr_t>(pointer);
        const auto begin = reinterpret_cast<std::uintptr_t>(values_.data());
        const auto bytes = values_.size() * sizeof(client_textmessage_t);
        return pointer && address >= begin && address - begin < bytes
            && (address - begin) % sizeof(client_textmessage_t) == 0;
    }
private:
    std::vector<TitleRecord> records_;
    std::vector<client_textmessage_t> values_;
};

class TitleSnapshots {
public:
    bool Prepare(const std::string& english, const std::string& selected) {
        prepared_ = nullptr;
        std::string key = english;
        key.push_back('\0');
        key += selected;
        const auto cached = snapshots_.find(key);
        if (cached != snapshots_.end()) {
            prepared_ = cached->second.get();
            return true;
        }
        std::vector<TitleRecord> records;
        std::vector<TitleRecord> overlay;
        if (!ParseTitleRecords(english, records) || records.empty() || !ParseTitleRecords(selected, overlay)) {
            return false;
        }
        std::map<std::string, std::size_t> indices;
        for (std::size_t index = 0; index < records.size(); ++index) {
            indices[TitleKey(records[index].name)] = index;
        }
        for (auto& record : overlay) {
            const std::string name = TitleKey(record.name);
            const auto found = indices.find(name);
            if (found == indices.end()) {
                indices[name] = records.size();
                records.push_back(std::move(record));
            } else {
                records[found->second] = std::move(record);
            }
        }
        auto snapshot = std::make_unique<TitleSnapshot>(std::move(records));
        prepared_ = snapshot.get();
        snapshots_.emplace(std::move(key), std::move(snapshot));
        return true;
    }

    void Commit() {
        active_ = prepared_;
        prepared_ = nullptr;
    }

    void Discard() {
        prepared_ = nullptr;
    }

    TitleSnapshot* Active() const {
        return active_;
    }

    bool IsCatalogue(const client_textmessage_t* pointer) const {
        for (const auto& snapshot : snapshots_) {
            if (snapshot.second->Contains(pointer)) {
                return true;
            }
        }
        return false;
    }

    void Clear() {
        prepared_ = nullptr;
        active_ = nullptr;
        snapshots_.clear();
    }
private:
    std::map<std::string, std::unique_ptr<TitleSnapshot>> snapshots_;
    TitleSnapshot* prepared_ = nullptr;
    TitleSnapshot* active_ = nullptr;
};

} // namespace localization
} // namespace cse

#endif
