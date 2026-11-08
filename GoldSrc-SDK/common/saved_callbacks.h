#pragma once

#include <cstddef>
#include <deque>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

namespace cse::save {

enum class CallbackStatus { NotReady, Found, Empty, Missing, Ambiguous };

template<typename Callback>
constexpr bool IsCallback = std::is_member_function_pointer_v<Callback>
    || (std::is_pointer_v<Callback> && std::is_function_v<std::remove_pointer_t<Callback>>);

// Each registry belongs to one callback signature/field category in one module.
// Populate it eagerly and seal it before restoration. Reverse lookup requires
// nonvirtual member functions: C++ does not define portable virtual-member
// equality. Current game assignment catalogs use nonvirtual dispatch wrappers.
// Names returned after sealing stay valid for this registry's lifetime.
// No member pointer is converted to an address or compared as object bytes.
template<typename Callback>
class CallbackRegistry {
    static_assert(IsCallback<Callback>, "A typed function or member-function pointer is required");

public:
    struct Match {
        CallbackStatus status = CallbackStatus::Missing;
        const char *name = nullptr;
        Callback value = nullptr;
    };

    CallbackRegistry() = default;
    CallbackRegistry(const CallbackRegistry &) = delete;
    CallbackRegistry &operator=(const CallbackRegistry &) = delete;

    bool Register(std::string_view name, Callback value) {
        if (sealed_ || !ValidName(name) || value == nullptr) {
            return false;
        }
        const auto existing = names_.find(name);
        if (existing != names_.end()) {
            const Entry &entry = entries_[existing->second];
            return entry.name == name && entry.value == value;
        }
        const auto index = entries_.size();
        entries_.push_back({std::string(name), value});
        try {
            names_.emplace(entries_.back().name, index);
        } catch (...) {
            entries_.pop_back();
            throw;
        }
        return true;
    }

    bool Alias(std::string_view alias, std::string_view canonical) {
        if (sealed_ || !ValidName(alias)) {
            return false;
        }
        const auto target = names_.find(canonical);
        if (target == names_.end()) {
            return false;
        }
        const auto existing = names_.find(alias);
        if (existing != names_.end()) {
            return existing->second == target->second;
        }
        const auto index = target->second;
        aliases_.emplace_back(alias);
        try {
            names_.emplace(aliases_.back(), index);
        } catch (...) {
            aliases_.pop_back();
            throw;
        }
        return true;
    }

    bool Seal() {
        sealed_ = true;
        return true;
    }

    Match Resolve(std::string_view name) const {
        if (!sealed_) {
            return {CallbackStatus::NotReady};
        }
        if (name.empty()) {
            return {CallbackStatus::Empty};
        }
        const auto found = names_.find(name);
        return found == names_.end() ? Match{CallbackStatus::Missing} : Found(entries_[found->second]);
    }

    // ICF can give distinct nonvirtual callbacks equal values. Never silently
    // select one by registration order. The owner may supply an independently
    // established canonical hint, for example an audited entity-context rule.
    Match NameFor(Callback value, std::string_view canonicalHint = {}) const {
        if (!sealed_) {
            return {CallbackStatus::NotReady};
        }
        if (value == nullptr) {
            return {CallbackStatus::Empty};
        }
        if (!canonicalHint.empty()) {
            const auto hinted = Resolve(canonicalHint);
            return hinted.status == CallbackStatus::Found && hinted.value == value
                ? hinted : Match{CallbackStatus::Missing};
        }
        const Entry *match = nullptr;
        for (const Entry &entry : entries_) {
            if (entry.value == value) {
                if (match) {
                    return {CallbackStatus::Ambiguous};
                }
                match = &entry;
            }
        }
        return match ? Found(*match) : Match{CallbackStatus::Missing};
    }

private:
    struct Entry {
        std::string name;
        Callback value;
    };

    static bool ValidName(std::string_view name) {
        return !name.empty() && name.find('\0') == std::string_view::npos;
    }

    static Match Found(const Entry &entry) {
        return {CallbackStatus::Found, entry.name.c_str(), entry.value};
    }

    // Deques keep string storage stable during catalog construction, including
    // short strings. All map views refer to owned names, never caller buffers.
    std::deque<Entry> entries_;
    std::deque<std::string> aliases_;
    std::unordered_map<std::string_view, std::size_t> names_;
    bool sealed_ = false;
};

template<typename Callback>
struct CallbackField {
    static_assert(IsCallback<Callback>, "A typed callback field is required");
    static constexpr std::size_t NativeStride = sizeof(Callback);

    static bool IsEmpty(const Callback &value) {
        return value == nullptr;
    }

    static void Clear(Callback &value) {
        value = nullptr;
    }

    static bool Restore(Callback &value, std::string_view name, const CallbackRegistry<Callback> &registry) {
        const auto match = registry.Resolve(name);
        if (match.status == CallbackStatus::Found) {
            value = match.value;
            return true;
        }
        Clear(value);
        return match.status == CallbackStatus::Empty;
    }
};

} // namespace cse::save
