#pragma once

// Light declarations for legacy translation units; the full descriptor and
// compiler-derived layout facts are consumed only in C++17 implementation code.
namespace cse { namespace abi { struct Descriptor; } }

#if defined(_WIN32)
#define CSE_GAME_ENTRY_CALL __cdecl
#define CSE_GAME_GIVE_CALL __stdcall
#else
#define CSE_GAME_ENTRY_CALL
#define CSE_GAME_GIVE_CALL
#endif

using CseInitializeGame = unsigned int (CSE_GAME_ENTRY_CALL *)(const cse::abi::Descriptor *,
    enginefuncs_t *, globalvars_t *);
bool CseGameEngineAbiBound();

// Used only by the legacy entry during a validated synchronous exchange.
bool CseGameConsumeEngineExchangeAuthorization();
