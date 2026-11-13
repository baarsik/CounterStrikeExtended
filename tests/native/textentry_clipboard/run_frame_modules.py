import argparse
import hashlib
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--repo", type=Path, required=True)
parser.add_argument("--out", type=Path, required=True)
parser.add_argument("--mode", choices=["Release", "Debug", "Sanitizers"], required=True)
parser.add_argument("--helper-root", type=Path)
parser.add_argument("--expect-flat-failure", action="store_true")
args = parser.parse_args()
args.out.mkdir(parents=True, exist_ok=True)
here = Path(__file__).resolve().parent
helper_root = args.helper_root or args.repo / "src/common"
flags = ["-std=c++14", "-arch", "arm64", "-mmacosx-version-min=14.0", "-fvisibility-inlines-hidden",
         "-fexceptions", "-fno-fast-math", "-ffp-contract=off", "-Wall", "-Wextra", "-Werror"]
flags += ["-O2"] if args.mode == "Release" else ["-O0", "-g"]
if args.mode == "Sanitizers":
    flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
rows = []
for binding, extra in [("twolevel", []), ("flat", ["-flat_namespace"])]:
    for identity in ["a", "b"]:
        image = args.out / (binding + "-" + identity + ".dylib")
        command = ["/usr/bin/clang++"] + flags + ["-dynamiclib"] + extra + [
            "-DCSE_FRAME_API_NAME=cse_test_frame_api_" + identity,
            "-I" + str(helper_root), str(here / "frame_module.cpp"),
            str(helper_root / "TextEntryClipboard.cpp"), "-o", str(image)]
        result = subprocess.run(command, capture_output=True)
        (args.out / (binding + "-" + identity + "-compile.log")).write_bytes(result.stdout + result.stderr)
        assert result.returncode == 0, result.stderr
        symbols = subprocess.run(["/usr/bin/nm", "-m", str(image)], capture_output=True, check=True)
        (args.out / (binding + "-" + identity + "-symbols.txt")).write_bytes(symbols.stdout)
        exports = subprocess.run(["/usr/bin/xcrun", "dyld_info", "-exports", str(image)], capture_output=True)
        (args.out / (binding + "-" + identity + "-exports.txt")).write_bytes(exports.stdout + exports.stderr)
        assert exports.returncode == 0, exports.stderr
        if not args.expect_flat_failure:
            assert b"cse_textentry_clipboard" not in exports.stdout, "private helper symbol exported"
        assert b"cse_test_frame_api" in exports.stdout
        private_exports = [line for line in exports.stdout.decode().splitlines() if "cse_textentry_clipboard" in line]
        rows.append({"command": command, "compile_exit": result.returncode,
                     "image_sha256": hashlib.sha256(image.read_bytes()).hexdigest(),
                     "private_exports": private_exports})
command = ["/usr/bin/clang++"] + flags + [str(here / "frame_modules_test.cpp"), "-o", str(args.out / "probe")]
subprocess.run(command, check=True)
for binding in ["twolevel", "flat"]:
    command = [str(args.out / "probe"), str(args.out / (binding + "-a.dylib")), str(args.out / (binding + "-b.dylib"))]
    result = subprocess.run(command, capture_output=True)
    (args.out / (binding + "-run.log")).write_bytes(result.stdout + result.stderr)
    expected = 1 if args.expect_flat_failure and binding == "flat" else 0
    assert result.returncode == expected, result.stdout + result.stderr
    rows.append({"command": command, "runtime_exit": result.returncode,
                 "expected_exit": expected, "independent_active_chains": result.returncode == 0})
(args.out / "commands.json").write_text(json.dumps(rows, indent=2) + "\n")
print(args.mode, "distinct-public-API two-image binding controls match expected results")
