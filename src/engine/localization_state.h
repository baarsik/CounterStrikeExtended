#ifndef CSE_ENGINE_LOCALIZATION_STATE_H
#define CSE_ENGINE_LOCALIZATION_STATE_H

#include "../public/localization_service.h"
#include "../common/interface_language.h"
#include <array>
#include <string>

namespace cse {
namespace localization {

class StateBackend {
public:
    virtual ~StateBackend() = default;
    virtual std::string Resolve(const std::string& preference) = 0;
    virtual bool Prepare(const std::string& language, std::string& error) = 0;
    virtual bool Persist(const std::string& preference, std::string& error) = 0;
    virtual void Commit(const std::string& language, std::uint64_t revision) = 0;
    virtual void Discard() = 0;
};

class State {
public:
    void Initialize(const std::string& language, const std::string& preference) {
        language_ = language;
        preference_ = preference;
        revision_ = 1;
        pending_ = InvalidRequest;
        lastError_.clear();
    }

    const std::string& Language() const {
        return language_;
    }

    const std::string& Preference() const {
        return preference_;
    }

    std::uint64_t Revision() const {
        return revision_;
    }

    const char* LastError() const {
        return lastError_.c_str();
    }

    std::uint64_t Request(const char* preference, bool persist) {
        const std::string normalized = interface_language::Normalize(preference);
        if (!preference || (preference[0] && !interface_language::IsBundled(normalized))) {
            lastError_ = "Choose a bundled interface language or the automatic choice.";
            return InvalidRequest;
        }
        if (pending_ != InvalidRequest) {
            SetResult(pending_, RequestState::Superseded);
        }
        ++serial_;
        if (serial_ == InvalidRequest) {
            ++serial_;
        }
        pending_ = serial_;
        pendingPreference_ = normalized;
        pendingPersist_ = persist;
        SetResult(pending_, RequestState::Pending);
        return pending_;
    }

    bool Result(std::uint64_t serial, RequestResult* result) const {
        if (!result || result->size < sizeof(RequestResult) || serial == InvalidRequest) {
            return false;
        }
        const Entry& entry = results_[serial % ResultHistorySize];
        if (entry.serial != serial) {
            return false;
        }
        *result = entry.result;
        return true;
    }

    void Process(StateBackend& backend) {
        if (pending_ == InvalidRequest || processing_) {
            return;
        }
        processing_ = true;
        const std::uint64_t request = pending_;
        const std::string preference = pendingPreference_;
        const bool persist = pendingPersist_;
        pending_ = InvalidRequest;
        const std::string language = backend.Resolve(preference);
        const bool changed = language != language_;
        std::string error;
        if (changed && !backend.Prepare(language, error)) {
            backend.Discard();
            Fail(request, error);
            processing_ = false;
            return;
        }
        if (persist && !backend.Persist(preference, error)) {
            if (changed) {
                backend.Discard();
            }
            Fail(request, error);
            processing_ = false;
            return;
        }
        if (changed) {
            const std::uint64_t nextRevision = revision_ + 1;
            backend.Commit(language, nextRevision);
            language_ = language;
            revision_ = nextRevision;
        }
        preference_ = preference;
        lastError_.clear();
        SetResult(request, RequestState::Applied);
        processing_ = false;
    }

private:
    // Enough history for outstanding UI requests without retaining unbounded records.
    static constexpr std::size_t ResultHistorySize = 32;
    struct Entry {
        std::uint64_t serial = InvalidRequest;
        RequestResult result{sizeof(RequestResult), RequestState::Pending, 0};
    };
    void SetResult(std::uint64_t serial, RequestState status) {
        results_[serial % ResultHistorySize] = {serial, {sizeof(RequestResult), status, revision_}};
    }

    void Fail(std::uint64_t request, const std::string& error) {
        lastError_ = error.empty() ? "The interface language could not be prepared." : error;
        SetResult(request, RequestState::Failed);
    }

    std::array<Entry, ResultHistorySize> results_{};
    std::string language_ = "english";
    std::string preference_;
    std::string pendingPreference_;
    std::string lastError_;
    std::uint64_t revision_ = 0;
    std::uint64_t serial_ = InvalidRequest;
    std::uint64_t pending_ = InvalidRequest;
    bool pendingPersist_ = false;
    bool processing_ = false;
};

} // namespace localization
} // namespace cse

#endif
