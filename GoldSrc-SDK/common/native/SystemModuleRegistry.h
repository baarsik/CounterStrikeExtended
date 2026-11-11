#pragma once

#include "../../public/native_system_module_abi.h"
#include <algorithm>
#include <limits>
#include <list>
#include <new>
#include <vector>

namespace cse {
namespace modules {

// Records outlive deregistration. Allocations stay in the provider until all callbacks
// and shutdown frames have returned; the owner unloads libraries only after ReleaseAll.
class Registry {
public:
    enum class State { Initializing, Running, Removing, Removed };
    struct Record {
        ISystemModule *module;
        const Factory *provider;
        State state;
        int serial;
        ISystemModule *parent;
        unsigned int activeFrames = 0;
        unsigned int publicationOrder = 0;
    };
    static constexpr unsigned int LastSerial = static_cast<unsigned int>(std::numeric_limits<int>::max());

    bool CanCreate() const {
        return !stopping && m_NextSerial <= LastSerial;
    }

    Lifetime lifetime;
    bool stopping = false;

    Record *Find(ISystemModule *module) {
        for (auto &record : m_Records) {
            if (record.module == module && record.state != State::Removed) {
                return &record;
            }
        }
        return nullptr;
    }

    template <class Cleanup>
    bool Add(IBaseSystem *system, ISystemModule *module, char *name, const Factory *provider, Cleanup cleanup) {
        if (!module || !CanCreate() || Find(module)) {
            return false;
        }
        auto old = std::find_if(m_Records.begin(), m_Records.end(),
                                [module](const Record &record) { return record.module == module; });
        if (old != m_Records.end() && (old->activeFrames || (provider && provider != old->provider))) {
            return false;
        }
        const int serial = static_cast<int>(m_NextSerial++);
        Record *record;
        if (old == m_Records.end()) {
            m_Records.push_back({module, provider, State::Initializing, serial, m_Initializing});
            record = &m_Records.back();
        } else {
            // Public AddModule passes null for a borrowed view; it must not cancel
            // the original provider's release obligation when reactivating that view.
            record = &*old;
            record->state = State::Initializing;
            record->serial = serial;
            record->parent = m_Initializing;
            record->publicationOrder = 0;
        }
        CallbackScope callback(&lifetime);
        RecordFrame frame(*record);
        struct InitializingScope {
            ISystemModule *&current;
            ISystemModule *previous;
            ~InitializingScope() {
                current = previous;
            }
        } initializing{m_Initializing, m_Initializing};
        m_Initializing = module;
        bool initialized = false;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try {
#endif
            initialized = module->Init(system, serial, name);
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        } catch (const std::bad_alloc &) {
            initialized = false;
        }
#endif
        if (!initialized || record->state != State::Initializing || stopping) {
            Remove(module, cleanup);
            return false;
        }
        record->state = State::Running;
        // Match legacy AddHead publication order without moving records or changing
        // frame iteration. Successful publications cannot exceed the serial limit.
        record->publicationOrder = m_NextPublication++;
        return true;
    }

    template <class Cleanup> bool Remove(ISystemModule *module, Cleanup cleanup) {
        Record *record = Find(module);
        if (!record || record->state == State::Removing) {
            return true;
        }
        record->state = State::Removing;
        CallbackScope callback(&lifetime);
        RecordFrame frame(*record);
        for (auto &other : m_Records) {
            if (other.state != State::Removed) {
                other.module->RemoveListener(module);
            }
        }
        cleanup(module);
        module->ShutDown();
        record->state = State::Removed;
        // Modules first acquired during Init belong to that transaction. Existing
        // cached modules keep their original owner and are never swept by rollback.
        for (auto &child : m_Records) {
            if (child.parent == module && child.state != State::Removed && child.state != State::Removing) {
                Remove(child.module, cleanup);
            }
        }
        return true;
    }

    template <class Cleanup> void Stop(Cleanup cleanup) {
        stopping = true;
        while (true) {
            Record *next = nullptr;
            for (auto &record : m_Records) {
                if (record.state != State::Running && record.state != State::Initializing) {
                    continue;
                }
                if (!next || record.publicationOrder > next->publicationOrder ||
                    (record.publicationOrder == next->publicationOrder && record.serial > next->serial)) {
                    next = &record;
                }
            }
            if (!next) {
                break;
            }
            // Recompute after each removal because callbacks can remove other records.
            Remove(next->module, cleanup);
        }
    }

    bool ReleaseAll() {
        if (lifetime.depth) {
            return false;
        }
        for (const auto &record : m_Records) {
            if (record.state != State::Removed) {
                return false;
            }
        }
        CallbackScope releasing(&lifetime);
        for (auto &record : m_Records) {
            if (record.provider) {
                record.provider->Release(record.module);
            }
        }
        m_Records.clear();
        return true;
    }

    std::vector<ISystemModule *> Running() const {
        std::vector<ISystemModule *> modules;
        for (const auto &record : m_Records) {
            if (record.state == State::Running) {
                modules.push_back(record.module);
            }
        }
        return modules;
    }

    bool Running(ISystemModule *module) {
        Record *record = Find(module);
        return record && record->state == State::Running;
    }

    void Restart() {
        stopping = false;
    }

private:
    // State can become Removed before an outer Init or recursive removal returns.
    // Keep that node stable and forbid reactivation until all owning frames unwind.
    struct RecordFrame {
        Record &record;
        explicit RecordFrame(Record &record) : record(record) {
            ++record.activeFrames;
        }
        ~RecordFrame() {
            --record.activeFrames;
        }
    };

    std::list<Record> m_Records;
    unsigned int m_NextSerial = 1;
    unsigned int m_NextPublication = 1;
    ISystemModule *m_Initializing = nullptr;
};

} // namespace modules
} // namespace cse
