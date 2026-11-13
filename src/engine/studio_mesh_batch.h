#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace cse::studio {

struct MeshVertex {
    std::int16_t position;
    std::int16_t normal;
    std::int16_t s;
    std::int16_t t;

    bool operator==(const MeshVertex& other) const {
        return position == other.position && normal == other.normal && s == other.s && t == other.t;
    }
};

struct MeshTopology {
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::uint32_t lastVertex = 0;
};

// Only owned data survives a draw. A cache address is an identity hint, never a
// borrowed pointer to dereference later: Cache_Move/eviction can replace models.
class MeshCache {
public:
    explicit MeshCache(std::size_t byteLimit = DefaultByteLimit) : byteLimit_(byteLimit) {}

    const MeshTopology* Get(const std::int16_t* commands, std::size_t availableWords,
        int vertexCount, int normalCount) {
        if (!commands || availableWords == 0 || vertexCount <= 0 || normalCount <= 0) {
            return nullptr;
        }
        const auto identity = reinterpret_cast<std::uintptr_t>(commands);
        auto found = entries_.find(identity);
        if (found != entries_.end()) {
            const auto& entry = found->second;
            if (entry.vertexCount == vertexCount && entry.normalCount == normalCount &&
                entry.commands.size() <= availableWords &&
                std::memcmp(entry.commands.data(), commands, entry.commands.size() * sizeof(*commands)) == 0) {
                return &entry.topology;
            }
            bytes_ -= entry.bytes;
            entries_.erase(found);
        }
        try {
            Entry entry;
            entry.vertexCount = vertexCount;
            entry.normalCount = normalCount;
            if (!Build(commands, availableWords, entry)) {
                return nullptr;
            }
            entry.bytes = entry.commands.capacity() * sizeof(std::int16_t) +
                entry.topology.vertices.capacity() * sizeof(MeshVertex) +
                entry.topology.indices.capacity() * sizeof(std::uint32_t) + sizeof(Entry);
            if (entry.bytes > byteLimit_) {
                return nullptr;
            }
            // Bounded wholesale eviction avoids an allocation/LRU update on warm draws.
            if (entries_.size() >= MaximumEntries || entry.bytes > byteLimit_ - bytes_) {
                Clear();
            }
            const auto inserted = entries_.emplace(identity, std::move(entry));
            bytes_ += inserted.first->second.bytes;
            return &inserted.first->second.topology;
        } catch (const std::bad_alloc&) {
            return nullptr;
        } catch (const std::length_error&) {
            return nullptr;
        }
    }

    void Clear() {
        entries_.clear();
        bytes_ = 0;
    }

private:
    static constexpr std::size_t DefaultByteLimit = 8 * 1024 * 1024;
    static constexpr std::size_t MaximumEntries = 512;
    // GoldSrc's retained studio triangle domain; cap malformed streams before allocation.
    static constexpr std::size_t MaximumTriangles = 20000;
    static constexpr std::size_t MaximumCommandVertices = MaximumTriangles * 3;
    static constexpr std::size_t WordsPerVertex = 4;

    struct VertexHash {
        std::size_t operator()(const MeshVertex& vertex) const {
            std::uint64_t bits = static_cast<std::uint16_t>(vertex.position);
            bits |= static_cast<std::uint64_t>(static_cast<std::uint16_t>(vertex.normal)) << 16;
            bits |= static_cast<std::uint64_t>(static_cast<std::uint16_t>(vertex.s)) << 32;
            bits |= static_cast<std::uint64_t>(static_cast<std::uint16_t>(vertex.t)) << 48;
            return std::hash<std::uint64_t>{}(bits);
        }
    };

    struct Entry {
        MeshTopology topology;
        std::vector<std::int16_t> commands;
        int vertexCount = 0;
        int normalCount = 0;
        std::size_t bytes = 0;
    };

    bool Build(const std::int16_t* commands, std::size_t availableWords, Entry& entry) const {
        std::unordered_map<MeshVertex, std::uint32_t, VertexHash> unique;
        std::size_t cursor = 0;
        std::size_t commandVertices = 0;
        while (cursor < availableWords) {
            const int signedCount = commands[cursor++];
            if (signedCount == 0) {
                entry.commands.assign(commands, commands + cursor);
                return true;
            }
            const auto count = static_cast<std::size_t>(signedCount < 0 ? -signedCount : signedCount);
            if (count > (availableWords - cursor) / WordsPerVertex ||
                count > MaximumCommandVertices - commandVertices) {
                return false;
            }
            commandVertices += count;
            if (count >= 3 && count - 2 > MaximumTriangles - entry.topology.indices.size() / 3) {
                return false;
            }
            std::uint32_t first = 0;
            std::uint32_t previous = 0;
            std::uint32_t beforePrevious = 0;
            for (std::size_t index = 0; index < count; ++index, cursor += WordsPerVertex) {
                const MeshVertex vertex{commands[cursor], commands[cursor + 1],
                    commands[cursor + 2], commands[cursor + 3]};
                if (vertex.position < 0 || vertex.position >= entry.vertexCount ||
                    vertex.normal < 0 || vertex.normal >= entry.normalCount) {
                    return false;
                }
                const auto candidate = static_cast<std::uint32_t>(entry.topology.vertices.size());
                const auto inserted = unique.emplace(vertex, candidate);
                if (inserted.second) {
                    entry.topology.vertices.push_back(vertex);
                }
                const auto current = inserted.first->second;
                if (index == 0) {
                    first = current;
                }
                if (index >= 2) {
                    // Keep the newest vertex last: GL_FLAT uses it for both legacy
                    // strips/fans and triangles, even when odd strip winding flips.
                    const bool oddStrip = signedCount > 0 && index % 2 != 0;
                    entry.topology.indices.push_back(signedCount < 0 ? first :
                        oddStrip ? previous : beforePrevious);
                    entry.topology.indices.push_back(oddStrip ? beforePrevious : previous);
                    entry.topology.indices.push_back(current);
                }
                beforePrevious = previous;
                previous = current;
                entry.topology.lastVertex = current;
            }
        }
        return false;
    }

    std::unordered_map<std::uintptr_t, Entry> entries_;
    std::size_t byteLimit_;
    std::size_t bytes_ = 0;
};

}
