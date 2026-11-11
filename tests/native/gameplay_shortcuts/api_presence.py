"""API-absence diagnostic, not a gameplay behavior regression test."""
import ctypes
import sys
provider = ctypes.CDLL(sys.argv[1])
try:
    policy = provider.CseSdl_SetCocoaGameplayShortcutsV1
except AttributeError:
    print("API-ABSENCE RED: actual provider has no CseSdl_SetCocoaGameplayShortcutsV1")
    sys.exit(17)
print("API present; this check does not test gameplay behavior")
