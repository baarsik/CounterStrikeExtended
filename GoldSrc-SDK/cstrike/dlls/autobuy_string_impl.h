#pragma once

#include <cstddef>
#include <cstring>

void CBasePlayer::AddAutoBuyData(const char *data) {
    auto length = std::strlen(m_autoBuyString);
    constexpr std::size_t SeparatorAndTerminatorBytes = 2;
    if (length <= sizeof(m_autoBuyString) - SeparatorAndTerminatorBytes) {
        if (length > 0) {
            m_autoBuyString[length++] = ' ';
            m_autoBuyString[length] = '\0';
        }
        std::strncat(m_autoBuyString, data, sizeof(m_autoBuyString) - length - 1);
    }
}
