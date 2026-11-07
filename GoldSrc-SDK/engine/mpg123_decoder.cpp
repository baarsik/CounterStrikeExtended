#include "mpg123_decoder.h"

#include <climits>
#include <cstdio>

namespace cse {

Mpg123Decoder::~Mpg123Decoder() {
    Close();
}

bool Mpg123Decoder::Open(const char *filename) {
    Close();
    if (!filename || !filename[0]) {
        return false;
    }
    // mpg123 1.33.7 requires no global init/exit; handles own decoder resources.
    handle_ = mpg123_new(nullptr, nullptr);
    if (!handle_) {
        return false;
    }
    const long flags = MPG123_GAPLESS | MPG123_QUIET | MPG123_NO_FRANKENSTEIN;
    const long *rates = nullptr;
    std::size_t rateCount = 0;
    mpg123_rates(&rates, &rateCount);
    bool valid = mpg123_param(handle_, MPG123_FLAGS, flags, 0.0) == MPG123_OK;
    valid = valid && mpg123_format_none(handle_) == MPG123_OK;
    for (std::size_t index = 0; valid && index < rateCount; ++index) {
        valid = mpg123_format(handle_, rates[index], MPG123_MONO | MPG123_STEREO,
                              MPG123_ENC_SIGNED_16) == MPG123_OK;
    }
    valid = valid && mpg123_open(handle_, filename) == MPG123_OK;
    long rate = 0;
    int channels = 0;
    int encoding = 0;
    valid = valid && mpg123_getformat(handle_, &rate, &channels, &encoding) == MPG123_OK;
    constexpr int maximumMusicChannels = 2;
    if (!valid || rate <= 0 || rate > INT_MAX || channels < 1 || channels > maximumMusicChannels ||
        encoding != MPG123_ENC_SIGNED_16) {
        Close();
        return false;
    }
    format_ = {SDL_AUDIO_S16, channels, static_cast<int>(rate)};
    return true;
}

void Mpg123Decoder::Close() {
    if (handle_) {
        mpg123_delete(handle_);
        handle_ = nullptr;
    }
    format_ = {};
}

Mpg123Decoder::Result Mpg123Decoder::Read(void *buffer, std::size_t capacity) {
    if (!handle_ || !buffer || capacity == 0) {
        return {0, false, true};
    }
    std::size_t bytes = 0;
    const int status = mpg123_read(handle_, buffer, capacity, &bytes);
    // A message result can still contain the last PCM bytes. Return both facts.
    if (status == MPG123_OK || status == MPG123_DONE) {
        return {bytes, status == MPG123_DONE, false};
    }
    if (status == MPG123_NEW_FORMAT) {
        long rate = 0;
        int channels = 0;
        int encoding = 0;
        const bool unchanged = mpg123_getformat(handle_, &rate, &channels, &encoding) == MPG123_OK &&
            rate == format_.freq && channels == format_.channels && encoding == MPG123_ENC_SIGNED_16;
        return {bytes, false, !unchanged};
    }
    return {bytes, false, true};
}

bool Mpg123Decoder::Rewind() {
    return handle_ && mpg123_seek64(handle_, 0, SEEK_SET) == 0;
}

SDL_AudioSpec Mpg123Decoder::Format() const {
    return format_;
}

}
