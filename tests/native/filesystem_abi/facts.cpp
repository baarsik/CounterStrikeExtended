#include "contract_spike.h"
#include <cstdio>

int main() {
    std::printf("{\"profile\":%llu,\"compiler_abi\":%u,\"pointer_bits\":%u,\"object_bytes\":%zu,"
        "\"object_alignment\":%zu,\"member_pointer_bytes\":%zu,\"long_bytes\":%zu,"
        "\"bool_bytes\":%zu,\"enum_bytes\":%zu,\"file_handle_bytes\":%zu,\"find_handle_bytes\":%zu,"
        "\"signature\":\"%016llx\",\"methods\":[",
        static_cast<unsigned long long>(filesystem_spike::Profile), cse::abi::NativeCompilerAbi,
        cse::abi::NativePointerBits, sizeof(IFileSystem), alignof(IFileSystem), sizeof(decltype(&IFileSystem::Mount)),
        sizeof(long), sizeof(bool), sizeof(FileSystemSeek_t), sizeof(FileHandle_t), sizeof(FileFindHandle_t),
        static_cast<unsigned long long>(filesystem_spike::ContractSignature()));
    for (std::size_t index = 0; index < filesystem_spike::PrefixMethods; ++index) {
        const auto& method = filesystem_spike::Methods[index];
        std::printf("%s{\"name\":\"%s\",\"ordinal\":%llu,\"slot\":%llu,\"shape\":\"%016llx\"}",
            index ? "," : "", method.name, static_cast<unsigned long long>(method.ordinal),
            static_cast<unsigned long long>(method.slot), static_cast<unsigned long long>(method.shape));
    }
    std::puts("]}");
}
