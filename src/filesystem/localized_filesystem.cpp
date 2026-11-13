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

std::uint32_t currentGroup = 0;
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

int FindAnchor(CBaseFileSystem* owner, const Group& group) {
    for (int index = 0; index < owner->m_SearchPaths.Count(); ++index) {
        const SearchPath& path = owner->m_SearchPaths[index];
        if (!path.m_bIsPackFile && path.m_Path.String() == group.anchor
            && path.m_PathID.String() == group.pathId) {
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
        if (FindAnchor(owner, group) < 0) {
            Discard();
            return nullptr;
        }
        currentGroup = replacement.group;
        for (std::size_t root = 0; root < replacement.count; ++root) {
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
        group.insertion = FindAnchor(owner, group);
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
    for (int index = owner->m_SearchPaths.Count() - 1; index >= 0; --index) {
        const auto id = owner->m_SearchPaths[index].m_localizationGroup;
        const bool managed = std::any_of(preparedGroups.begin(), preparedGroups.end(),
            [id](const Group& group) { return group.id == id; });
        if (managed) {
            owner->m_SearchPaths.Remove(index);
        }
    }
    for (auto& group : preparedGroups) {
        int insertion = FindAnchor(owner, group);
        for (auto& path : group.paths) {
            const int index = owner->m_SearchPaths.InsertBefore(insertion++);
            TransferPath(owner->m_SearchPaths[index], *path);
        }
    }
    Discard();
}

void BeginGroup(std::uint32_t group) {
    currentGroup = group;
}

void EndGroup() {
    currentGroup = 0;
}

IFileSystem* Baseline() {
    return preparedOwner ? &baselineView : nullptr;
}

const cse::localized_filesystem::Api api{
    sizeof(cse::localized_filesystem::Api), cse::localized_filesystem::Version,
    BeginGroup, EndGroup, Prepare, Baseline, Commit, Discard, Discard};

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
void ResetLocalizedFileSystem() {
    Discard();
}
} // namespace localized_filesystem
} // namespace cse
