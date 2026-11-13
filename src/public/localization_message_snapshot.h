#ifndef CSE_PUBLIC_LOCALIZATION_MESSAGE_SNAPSHOT_H
#define CSE_PUBLIC_LOCALIZATION_MESSAGE_SNAPSHOT_H

#include <string>
#include <utility>

namespace cse {
namespace localization {

// A message may originate in the engine title catalogue or in mutable network/demo
// storage. Classification is supplied by the owning engine, never inferred from text.
template<class Message>
class MessageSnapshot {
public:
    MessageSnapshot() = default;

    MessageSnapshot(const MessageSnapshot& other) {
        Copy(other);
    }

    MessageSnapshot& operator=(const MessageSnapshot& other) {
        if (this != &other) {
            Copy(other);
        }
        return *this;
    }

    MessageSnapshot(MessageSnapshot&& other) {
        Copy(other);
        other.Clear();
    }

    MessageSnapshot& operator=(MessageSnapshot&& other) {
        if (this != &other) {
            Copy(other);
            other.Clear();
        }
        return *this;
    }

    void Assign(const Message* source, bool catalogueSource) {
        if (!source) {
            Clear();
            return;
        }
        const std::string name = source->pName ? source->pName : "";
        const std::string text = source->pMessage ? source->pMessage : "";
        const bool hasName = source->pName != nullptr;
        const bool hasText = source->pMessage != nullptr;
        m_message = *source;
        m_name = name;
        m_text = text;
        m_hasName = hasName;
        m_hasText = hasText;
        m_catalogueSource = catalogueSource && hasName && !name.empty();
        m_present = true;
        Rebind();
    }

    bool Relocalize(Message* (*lookup)(const char*)) {
        if (!m_present || !m_catalogueSource || !lookup) {
            return false;
        }
        const Message* replacement = lookup(m_name.c_str());
        if (!replacement || !replacement->pMessage) {
            return false;
        }
        // Preserve animation/style/timing already applied to an active message.
        m_text = replacement->pMessage;
        m_hasText = true;
        Rebind();
        return true;
    }

    Message* Get() {
        return m_present ? &m_message : nullptr;
    }

    const Message* Get() const {
        return m_present ? &m_message : nullptr;
    }

    void Clear() {
        m_message = Message{};
        m_name.clear();
        m_text.clear();
        m_hasName = false;
        m_hasText = false;
        m_catalogueSource = false;
        m_present = false;
    }

    bool HasCatalogueSource() const {
        return m_catalogueSource;
    }

private:
    void Copy(const MessageSnapshot& other) {
        m_message = other.m_message;
        m_name = other.m_name;
        m_text = other.m_text;
        m_hasName = other.m_hasName;
        m_hasText = other.m_hasText;
        m_catalogueSource = other.m_catalogueSource;
        m_present = other.m_present;
        Rebind();
    }

    void Rebind() {
        m_message.pName = m_hasName ? m_name.c_str() : nullptr;
        m_message.pMessage = m_hasText ? m_text.c_str() : nullptr;
    }

    Message m_message{};
    std::string m_name;
    std::string m_text;
    bool m_hasName = false;
    bool m_hasText = false;
    bool m_catalogueSource = false;
    bool m_present = false;
};

} // namespace localization
} // namespace cse

#endif
