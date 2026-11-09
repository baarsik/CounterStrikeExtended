#ifndef CSE_RASTER_FIXTURE_DIGEST_H
#define CSE_RASTER_FIXTURE_DIGEST_H
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>
#if defined(_WIN32)
#include <Windows.h>
#include <bcrypt.h>
#else
#include <CommonCrypto/CommonDigest.h>
#endif

class Digest {
public:
    Digest() {
#if defined(_WIN32)
        Check(BCryptOpenAlgorithmProvider(&algorithm_, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0);
        ULONG bytes = 0;
        ULONG copied = 0;
        Check(BCryptGetProperty(algorithm_, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&bytes),
            sizeof(bytes), &copied, 0) >= 0);
        object_.resize(bytes);
        Check(BCryptCreateHash(algorithm_, &hash_, object_.data(), bytes, nullptr, 0, 0) >= 0);
#else
        Check(CC_SHA256_Init(&state_) == 1);
#endif
    }
    ~Digest() {
#if defined(_WIN32)
        Check(BCryptDestroyHash(hash_) >= 0 && BCryptCloseAlgorithmProvider(algorithm_, 0) >= 0);
#endif
    }
    void Update(const unsigned char* data, std::size_t bytes) {
#if defined(_WIN32)
        Check(bytes <= (std::numeric_limits<ULONG>::max)());
        Check(BCryptHashData(hash_, const_cast<unsigned char*>(data), static_cast<ULONG>(bytes), 0) >= 0);
#else
        Check(bytes <= (std::numeric_limits<CC_LONG>::max)());
        Check(CC_SHA256_Update(&state_, data, static_cast<CC_LONG>(bytes)) == 1);
#endif
    }
    std::string Finish() {
        std::array<unsigned char, 32> bytes{};
#if defined(_WIN32)
        Check(BCryptFinishHash(hash_, bytes.data(), static_cast<ULONG>(bytes.size()), 0) >= 0);
#else
        Check(CC_SHA256_Final(bytes.data(), &state_) == 1);
#endif
        constexpr char kDigits[] = "0123456789abcdef";
        std::string result;
        for (const auto value : bytes) {
            result += kDigits[value >> 4];
            result += kDigits[value & 15];
        }
        return result;
    }

private:
    static void Check(bool valid) {
        if (!valid) {
            std::fputs("FAILED fixture SHA256 operation\n", stderr);
            std::abort();
        }
    }
#if defined(_WIN32)
    BCRYPT_ALG_HANDLE algorithm_ = nullptr;
    BCRYPT_HASH_HANDLE hash_ = nullptr;
    std::vector<unsigned char> object_;
#else
    CC_SHA256_CTX state_{};
#endif
};

#endif
