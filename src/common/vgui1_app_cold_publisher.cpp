#if !defined(__APPLE__)
#error This unselected publisher currently emits only Apple Mach-O metadata
#endif
#if !defined(CSE_VGUI1_APP_COLD_GENERATED_HEADER)
#error Fresh actual owning compiler evidence is required
#endif
#if defined(CSE_VGUI1_APP_COLD_OFFER) == defined(CSE_VGUI1_APP_COLD_REQUIREMENT)
#error Select exactly one dependency-edge metadata role
#endif

#include "../public/cse_vgui1_app_cold_metadata.h"
#define CSE_VGUI1_APP_COLD_METADATA_PUBLISHER 1
#include CSE_VGUI1_APP_COLD_GENERATED_HEADER

namespace {
namespace metadata = cse::vgui1_app_cold_metadata;
namespace generated = cse::vgui1_app_cold_generated;
static_assert(generated::MetadataReady && !generated::ColdLoadReady,
    "Metadata evidence is not native service or constructor-lifetime admission");
#if defined(CSE_VGUI1_APP_COLD_OFFER)
constexpr auto Role = metadata::Kind::Offer;
#else
constexpr auto Role = metadata::Kind::Requirement;
#endif
constexpr auto Record = metadata::Encode(Role, generated::Provenance, generated::Catalog);
}

// Only constant bytes. Hidden, retained metadata is read from the file; no
// metadata getter, process query or image self-hash is added. This declaration
// is constant-initialized; inherited target force-includes can still contribute
// unrelated module initializers, which the exact owning-recipe gate records.
extern "C" __attribute__((used, visibility("hidden"), section("__DATA_CONST,__cse_vgui1")))
#if defined(__clang__)
#if __has_attribute(no_sanitize)
// This is a serialized on-disk record, not addressable application storage.
// ASan global redzones otherwise enlarge its file section from352 to416 bytes.
// Only this byte array is exempt; parser and all cold-owner code stay instrumented.
__attribute__((no_sanitize("address")))
#endif
#endif
const cse::vgui1_app_cold_metadata::EncodedRecord CseVgui1AppColdMetadata = Record;
