from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument("--repo", type=Path, required=True)
parser.add_argument("--sdl-source", type=Path, required=True)
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--evidence", type=Path, required=True)
args = parser.parse_args()
args.evidence.mkdir(parents=True, exist_ok=False)
original = (args.repo / "src/common/sdl3_native_mouse_bridge.c").read_text()
before_return = "    return SDL_GetRelativeMouseMode() ? CseSdlMouseModeRelative : CseSdlMouseModeAbsolute;"
changes = {
    "skip-identity": ("if (!CseSdl_UsesLocalDispatch())", "if (false && !CseSdl_UsesLocalDispatch())"),
    "compare-local-only": ("return SDL_GetDefaultAssertionHandler() == SDL_GetDefaultAssertionHandler_REAL();",
                           "return SDL_GetDefaultAssertionHandler_REAL() == SDL_GetDefaultAssertionHandler_REAL();"),
    "read-device-before-main": ("    // Use the stable SDK main-thread check", "    (void)SDL_GetVideoDevice();\n    // Use the stable SDK main-thread check"),
    "read-mode-before-identity": ("    // Public video initialization", "    (void)SDL_GetRelativeMouseMode();\n    // Public video initialization"),
    "consume-relative": (before_return, "    SDL_GetRelativeMouseState(NULL, NULL);\n" + before_return),
    "write-error": (before_return, '    SDL_SetError("deliberate fixture mutation");\n' + before_return),
    "allocate": (before_return, "    SDL_free(SDL_malloc(1));\n" + before_return),
    "use-current-handler": ("== SDL_GetDefaultAssertionHandler_REAL();", "== SDL_GetAssertionHandler_REAL(NULL);"),
    "invert-mode": (before_return, "    return SDL_GetRelativeMouseMode() ? CseSdlMouseModeAbsolute : CseSdlMouseModeRelative;"),
}
records = []
for name, (before, after) in changes.items():
    assert original.count(before) == 1, name
    generated = original.replace(before, after)
    source = args.evidence / (name + ".c")
    source.write_text(generated)
    build = args.build / name
    commands = [("configure", ["cmake", "-S", args.repo / "tests/native/sdl_mouse_bridge", "-B", build,
                "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug", "-DCSE_SDL_SOURCE_DIR=" + str(args.sdl_source),
                "-DCSE_BRIDGE_TEST_SOURCE=" + str(source)], 0),
                ("build", ["cmake", "--build", build, "--parallel", "2"], 0),
                ("run", [build / ("bridge_tests.exe" if sys.platform == "win32" else "bridge_tests")], 17)]
    if sys.platform == "darwin":
        commands[0][1].append("-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0")
    for phase, argv, expected in commands:
        result = subprocess.run([str(x) for x in argv], capture_output=True, text=True)
        (args.evidence / (name + "-" + phase + ".log")).write_text(result.stdout + result.stderr)
        records.append(dict(name=name, phase=phase, argv=[str(x) for x in argv], exit=result.returncode,
                            expected=expected, generated_sha256=hashlib.sha256(generated.encode()).hexdigest()))
        (args.evidence / "receipt.json").write_text(json.dumps(records, indent=2) + "\n")
        if result.returncode != expected:
            raise SystemExit(f"{name}/{phase}: expected {expected}, observed {result.returncode}")
