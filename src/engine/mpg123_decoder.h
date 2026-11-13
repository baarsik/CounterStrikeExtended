#pragma once

#include <SDL3/SDL_audio.h>
#include <mpg123.h>
#include <cstddef>

namespace cse {

// One decoder belongs to one producer. SDL owns device format conversion.
class Mpg123Decoder {
public:
    struct Result {
        std::size_t bytes;
        bool end;
        bool error;
    };

    Mpg123Decoder() = default;
    ~Mpg123Decoder();
    Mpg123Decoder(const Mpg123Decoder &) = delete;
    Mpg123Decoder &operator=(const Mpg123Decoder &) = delete;

    bool Open(const char *filename);
    void Close();
    Result Read(void *buffer, std::size_t capacity);
    bool Rewind();
    SDL_AudioSpec Format() const;

private:
    mpg123_handle *handle_ = nullptr;
    SDL_AudioSpec format_{};
};

}
