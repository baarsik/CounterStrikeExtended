#include "basefilesystem.h"
#include "localized_filesystem.h"
#include "../public/localized_filesystem.h"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <memory>
#include <new>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {
#include "localized_filesystem_view.h"

using SearchPath = CBaseFileSystem::CSearchPath;
using cse::localized_filesystem::Replacement;

struct Group {
    std::uint32_t id = 0;
    std::string anchor;
    std::string pathId;
    std::vector<std::unique_ptr<SearchPath>> paths;
    int insertion = 0;
};

struct MountBoundary {
    std::uint32_t group;
    std::string root;
    std::string pathId;
};

std::uint32_t currentGroup = 0;
std::vector<MountBoundary> mountBoundaries;
CBaseFileSystem* preparedOwner = nullptr;
std::vector<Group> preparedGroups;
ReadView preparedView;
ReadView baselineView;
struct PackCursor {
    FILE* stream;
    fpos_t position;
};
std::vector<PackCursor> originalPackCursors;

// CUtlVector relocates its elements with memmove. Transfer the same relocatable
// ownership here, then reconstruct the source so no pack/tree owner is copied.
void TransferPath(SearchPath& destination, SearchPath& source) {
    destination.~SearchPath();
    std::memcpy(static_cast<void*>(&destination), static_cast<void*>(&source), sizeof(SearchPath));
    new (&source) SearchPath();
}

std::string NormalizeRoot(CBaseFileSystem* owner, const char* root) {
    if (!root) {
        return {};
    }
    char normalized[FS_SEARCH_PATH_CAPACITY]{};
#if defined(_WIN32)
    const bool absolute = std::strchr(root, ':') != nullptr;
#else
    const bool absolute = root[0] == '/';
#endif
    if (absolute) {
        std::snprintf(normalized, sizeof(normalized), "%s", root);
    } else {
        owner->GetCurrentDirectory(normalized, sizeof(normalized));
        owner->FixPath(normalized);
        const std::size_t length = std::strlen(normalized);
        std::snprintf(normalized + length, sizeof(normalized) - length, "%s", root);
    }
#if defined(_WIN32)
    _strlwr(normalized);
#endif
    owner->FixPath(normalized);
    return normalized;
}

int FindAnchor(CBaseFileSystem* owner, const std::string& root, const std::string& pathId) {
    for (int index = 0; index < owner->m_SearchPaths.Count(); ++index) {
        const SearchPath& path = owner->m_SearchPaths[index];
        if (!path.m_bIsPackFile && path.m_Path.String() == root && path.m_PathID.String() == pathId) {
            return index;
        }
    }
    return -1;
}

void RestorePackCursors() {
    for (const auto& cursor : originalPackCursors) {
        std::fsetpos(cursor.stream, &cursor.position);
    }
    originalPackCursors.clear();
}

void Discard() {
    RestorePackCursors();
    preparedView.paths.clear();
    baselineView.paths.clear();
    preparedGroups.clear();
    preparedOwner = nullptr;
    preparedView.owner = nullptr;
    baselineView.owner = nullptr;
    currentGroup = 0;
}

IFileSystem* Prepare(IFileSystem* filesystem, const Replacement* replacements, std::size_t count) {
    Discard();
    auto* owner = static_cast<CBaseFileSystem*>(filesystem);
    if (!owner || owner != CBaseFileSystem::s_pFileSystem || (!replacements && count)) {
        return nullptr;
    }
    preparedOwner = owner;
    // Read views borrow active pack streams. Preserve their cursors so preparing
    // translations cannot consume bytes from an unrelated live file handle.
    for (int index = 0; index < owner->m_SearchPaths.Count(); ++index) {
        const auto& path = owner->m_SearchPaths[index];
        if (path.m_bIsPackFile && path.m_hPackFile && path.m_hPackFile->m_pFile) {
            PackCursor cursor{path.m_hPackFile->m_pFile, {}};
            if (std::fgetpos(cursor.stream, &cursor.position) == 0) {
                originalPackCursors.push_back(cursor);
            }
        }
    }
    std::set<std::uint32_t> ids;
    std::set<std::pair<std::string, std::string>> stagedRoots;
    for (std::size_t index = 0; index < count; ++index) {
        const Replacement& replacement = replacements[index];
        if (!replacement.group || !replacement.anchor || !replacement.pathId
            || (!replacement.roots && replacement.count) || !ids.insert(replacement.group).second) {
            Discard();
            return nullptr;
        }
        Group group;
        group.id = replacement.group;
        group.anchor = NormalizeRoot(owner, replacement.anchor);
        group.pathId = replacement.pathId;
        group.insertion = FindAnchor(owner, group.anchor, group.pathId);
        if (group.insertion < 0) {
            Discard();
            return nullptr;
        }
        currentGroup = replacement.group;
        for (std::size_t root = 0; root < replacement.count; ++root) {
            // Earlier groups were detached, so AddSearchPathInternal cannot see their loose roots or packs.
            if (!stagedRoots.emplace(NormalizeRoot(owner, replacement.roots[root]), group.pathId).second) {
                continue;
            }
            const int first = owner->m_SearchPaths.Count();
            owner->AddSearchPathInternal(replacement.roots[root], replacement.pathId, replacement.writable);
            const int last = owner->m_SearchPaths.Count();
            for (int pathIndex = first; pathIndex < last; ++pathIndex) {
                auto staged = std::make_unique<SearchPath>();
                TransferPath(*staged, owner->m_SearchPaths[pathIndex]);
                group.paths.push_back(std::move(staged));
            }
            while (owner->m_SearchPaths.Count() > first) {
                owner->m_SearchPaths.Remove(owner->m_SearchPaths.Count() - 1);
            }
        }
        currentGroup = 0;
        preparedGroups.push_back(std::move(group));
    }
    int capacity = owner->m_SearchPaths.Count();
    for (const auto& group : preparedGroups) {
        capacity += static_cast<int>(group.paths.size());
    }
    owner->m_SearchPaths.EnsureCapacity(capacity);
    preparedView.owner = owner;
    baselineView.owner = owner;
    for (auto& group : preparedGroups) {
        for (const auto& boundary : mountBoundaries) {
            if (boundary.group == group.id && !boundary.root.empty()) {
                const int index = FindAnchor(owner, boundary.root, boundary.pathId);
                if (index >= 0) {
                    group.insertion = index;
                }
                break;
            }
        }
        for (int index = 0; index < owner->m_SearchPaths.Count(); ++index) {
            if (owner->m_SearchPaths[index].m_localizationGroup == group.id) {
                group.insertion = index;
                break;
            }
        }
    }
    for (int index = 0; index <= owner->m_SearchPaths.Count(); ++index) {
        for (auto& group : preparedGroups) {
            if (group.insertion == index) {
                for (auto& path : group.paths) {
                    preparedView.paths.push_back(path.get());
                }
            }
        }
        if (index < owner->m_SearchPaths.Count()) {
            SearchPath* path = &owner->m_SearchPaths[index];
            if (!ids.count(path->m_localizationGroup)) {
                preparedView.paths.push_back(path);
            }
            if (!path->m_localizationGroup) {
                baselineView.paths.push_back(path);
            }
        }
    }
    return &preparedView;
}

void Commit() {
    CBaseFileSystem* owner = preparedOwner;
    if (!owner) {
        return;
    }
    RestorePackCursors();
    preparedView.paths.clear();
    baselineView.paths.clear();
    // Work backwards so each group's prepared insertion index still refers to its original position.
    for (int index = owner->m_SearchPaths.Count() - 1; index >= 0; --index) {
        const auto id = owner->m_SearchPaths[index].m_localizationGroup;
        const bool managed = std::any_of(preparedGroups.begin(), preparedGroups.end(),
            [id](const Group& group) { return group.id == id; });
        if (managed) {
            owner->m_SearchPaths.Remove(index);
        }
        // Groups sharing an anchor appear in the same order as they did in the read view.
        for (auto group = preparedGroups.rbegin(); group != preparedGroups.rend(); ++group) {
            if (group->insertion == index) {
                int insertion = index;
                for (auto& path : group->paths) {
                    owner->m_SearchPaths.InsertBefore(insertion);
                    TransferPath(owner->m_SearchPaths[insertion], *path);
                    ++insertion;
                }
            }
        }
    }
    Discard();
}

void BeginGroup(std::uint32_t group) {
    currentGroup = group;
}

void EndGroup() {
    mountBoundaries.push_back({currentGroup, {}, {}});
    currentGroup = 0;
}

void Reset() {
    Discard();
    mountBoundaries.clear();
}

IFileSystem* Baseline() {
    return preparedOwner ? &baselineView : nullptr;
}

const cse::localized_filesystem::Api api{
    sizeof(cse::localized_filesystem::Api), cse::localized_filesystem::Version,
    BeginGroup, EndGroup, Prepare, Baseline, Commit, Discard, Reset};

void* Factory() {
    return const_cast<cse::localized_filesystem::Api*>(&api);
}

InterfaceReg registration(Factory, cse::localized_filesystem::InterfaceName);
} // namespace

namespace cse {
namespace localized_filesystem {
std::uint32_t CurrentGroup() {
    return currentGroup;
}
void RecordFollowingMount(const char* root, const char* pathId) {
    if (currentGroup) {
        return;
    }
    // English groups can have no paths. Retain their position relative to the next actual mount,
    // including a BASE path installed before the group's named game-directory anchor.
    for (auto it = mountBoundaries.rbegin(); it != mountBoundaries.rend() && it->root.empty(); ++it) {
        it->root = root;
        it->pathId = pathId;
    }
}

void ResetLocalizedFileSystem() {
    Reset();
}
} // namespace localized_filesystem
} // namespace cse
