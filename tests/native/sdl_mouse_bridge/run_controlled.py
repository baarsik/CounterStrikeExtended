from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument("--repo", type=Path, required=True)
parser.add_argument("--sdl-source", type=Path, required=True)
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--evidence", type=Path, required=True)
parser.add_argument("--config", choices=["Release", "Debug"], required=True)
parser.add_argument("--sanitizers", action="store_true")
args = parser.parse_args()
args.evidence.mkdir(parents=True, exist_ok=False)
records = []
receipt = dict(config=args.config, sanitizers=args.sanitizers, commands=records,
               real_provider_or_game_executed=False, production_readiness=False)

def run(name, argv, expected=0):
    start = time.monotonic()
    result = subprocess.run([str(x) for x in argv], capture_output=True, text=True)
    (args.evidence / (name + ".log")).write_text(result.stdout + result.stderr)
    records.append(dict(name=name, argv=[str(x) for x in argv], exit=result.returncode,
                        expected=expected, seconds=time.monotonic() - start))
    (args.evidence / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    if result.returncode != expected:
        raise SystemExit(f"{name}: expected {expected}, observed {result.returncode}")

configure = ["cmake", "-S", args.repo / "tests/native/sdl_mouse_bridge", "-B", args.build,
             "-G", "Ninja", "-DCMAKE_BUILD_TYPE=" + args.config,
             "-DCSE_SDL_SOURCE_DIR=" + str(args.sdl_source),
             "-DCSE_BRIDGE_SANITIZERS=" + ("ON" if args.sanitizers else "OFF")]
if __import__("sys").platform == "darwin":
    configure.append("-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0")
run("configure", configure)
run("build", ["cmake", "--build", args.build, "--parallel", "2"])
suffix = ".exe" if __import__("os").name == "nt" else ""
run("header", [args.build / ("bridge_header_probe" + suffix)])
run("naive-authority-red", [args.build / ("bridge_naive_red" + suffix)], expected=17)
selected = (args.build / ("bridge_tests" + suffix)).exists()
receipt["native_bridge_selected"] = selected
if selected:
    run("actual-bridge", [args.build / ("bridge_tests" + suffix)])
receipt["success"] = True
executables = {"bridge_header_probe" + suffix, "bridge_naive_red" + suffix, "bridge_tests" + suffix}
receipt["images"] = [dict(path=str(path), bytes=path.stat().st_size,
                          sha256=hashlib.sha256(path.read_bytes()).hexdigest())
                     for path in sorted(args.build.iterdir())
                     if path.is_file() and (path.name in executables or
                        ("bridge_fixture" in path.name and path.suffix in [".dll", ".dylib", ".so"]))]
receipt["compile_commands"] = json.loads((args.build / "compile_commands.json").read_text())
(args.evidence / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
