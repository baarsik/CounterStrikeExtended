from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys
import threading
import time

parser = argparse.ArgumentParser()
parser.add_argument("--repo", type=Path, required=True)
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--evidence", type=Path, required=True)
parser.add_argument("--baseline", type=Path, required=True)
parser.add_argument("--config", choices=["Release", "Debug"], required=True)
parser.add_argument("--profile-budget", type=int, default=512 * 1024 * 1024)
args = parser.parse_args()
args.evidence.mkdir(parents=True, exist_ok=False)
args.build.mkdir(parents=True, exist_ok=True)
records = []
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
reserve = 4 * 1024**3
disk = dict(reserve_bytes=reserve, admission_budget_bytes=args.profile_budget, samples=0,
            initial_free_bytes=shutil.disk_usage(args.build).free, minimum_free_bytes=shutil.disk_usage(args.build).free,
            observed_owned_peak_bytes=0, transient_stat_misses=0)
if os.name == "nt" and disk["initial_free_bytes"] <= reserve + args.profile_budget:
    raise SystemExit("Insufficient measured profile budget plus 4 GiB reserve")
receipt = dict(config=args.config, commands=records, disk=disk, os_input_or_game_claim=False,
               driver="Existing upstream dummy video; no SDK feature option changed", production_readiness=False)
stop = threading.Event()

def sample():
    while not stop.is_set():
        disk["minimum_free_bytes"] = min(disk["minimum_free_bytes"], shutil.disk_usage(args.build).free)
        size = 0
        for path in args.build.rglob("*"):
            try:
                if path.is_file():
                    size += path.stat().st_size
            except OSError:
                disk["transient_stat_misses"] += 1
        disk["observed_owned_peak_bytes"] = max(disk["observed_owned_peak_bytes"], size)
        disk["samples"] += 1
        stop.wait(0.2)

monitor = threading.Thread(target=sample)
monitor.start()

def save():
    (args.evidence / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")

def run(name, argv, overrides=None, dll_directory=None):
    if os.name == "nt" and shutil.disk_usage(args.build).free < reserve:
        raise RuntimeError("4 GiB reserve unavailable before owned command")
    environment = os.environ.copy()
    environment.pop("SDL3_DYNAMIC_API", None)
    if overrides:
        environment.update(overrides)
    if dll_directory is not None and os.name == "nt":
        environment["PATH"] = str(dll_directory) + os.pathsep + environment.get("PATH", "")
    start = time.monotonic()
    with (args.evidence / (name + ".log")).open("w") as log:
        result = subprocess.run([str(x) for x in argv], env=environment, stdout=log,
                                stderr=subprocess.STDOUT, timeout=1800)
    records.append(dict(name=name, argv=[str(x) for x in argv], exit=result.returncode,
                        seconds=time.monotonic() - start, environment_overrides=overrides or {},
                        owned_dll_search_prefix=str(dll_directory) if dll_directory is not None else None))
    save()
    if result.returncode:
        raise RuntimeError(f"{name}: exit {result.returncode}")
    print(name + ":0", flush=True)

def exports(path):
    if os.name != "nt":
        text = subprocess.check_output(["nm", "-gU", str(path)], text=True)
        return {line.split()[-1] for line in text.splitlines() if line.strip()}
    data = path.read_bytes()
    pe = struct.unpack_from("<I", data, 60)[0]
    optional = pe + 24
    directory = optional + (112 if struct.unpack_from("<H", data, optional)[0] == 0x20b else 96)
    section_count = struct.unpack_from("<H", data, pe + 6)[0]
    optional_bytes = struct.unpack_from("<H", data, pe + 20)[0]
    sections = [struct.unpack_from("<IIII", data, optional + optional_bytes + index * 40 + 8)
                for index in range(section_count)]
    def offset(rva):
        for extent, start, raw, begin in sections:
            if start <= rva < start + raw:
                return begin + rva - start
        raise ValueError("Unmapped export RVA")
    rva = struct.unpack_from("<I", data, directory)[0]
    header = offset(rva)
    count = struct.unpack_from("<I", data, header + 24)[0]
    names = offset(struct.unpack_from("<I", data, header + 32)[0])
    result = set()
    for index in range(count):
        name = offset(struct.unpack_from("<I", data, names + index * 4)[0])
        result.add(data[name:data.index(b"\0", name)].decode("ascii"))
    return result

try:
    configure = ["cmake", "-S", args.repo / "tests/native/sdl_mouse_bridge/provider", "-B", args.build,
                 "-G", "Ninja", "-DCMAKE_BUILD_TYPE=" + args.config]
    if sys.platform == "darwin":
        configure.append("-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0")
    run("production-configure", configure + ["-DCSE_BRIDGE_FIXTURE_LOCAL_SEAMS=OFF"])
    run("production-build", ["cmake", "--build", args.build, "--parallel", "2"])
    suffix = ".exe" if os.name == "nt" else ""
    product = Path((args.build / ("provider-path-" + args.config + ".txt")).read_text().strip())
    product_sha = sha(product)
    public = exports(product)
    baseline = exports(args.baseline)
    bridge_name = ("" if os.name == "nt" else "_") + "CseSdl_QueryMouseModeV1"
    assert public - baseline == {bridge_name} and not baseline - public
    receipt["production"] = dict(path=str(product), bytes=product.stat().st_size, sha256=product_sha,
        exports=sorted(public), added=sorted(public - baseline), removed=sorted(baseline - public),
        baseline_path=str(args.baseline), baseline_sha256=sha(args.baseline))
    (args.evidence / "production-compile-commands.json").write_text((args.build / "compile_commands.json").read_text())
    produced = args.evidence / "production-images"
    produced.mkdir()
    for path in [product, args.build / ("bridge_import_probe" + suffix), args.build / ("bridge_provider_probe" + suffix)]:
        shutil.copy2(path, produced / path.name)
    run("import", [args.build / ("bridge_import_probe" + suffix)], dll_directory=product.parent)
    run("same", [args.build / ("bridge_provider_probe" + suffix), "same", product], {"SDL_VIDEODRIVER": "dummy"})
    objects = {str(path): sha(path) for path in (args.build / "dependencies/SDL3").rglob("*")
               if path.is_file() and path.suffix in [".o", ".obj"]}
    (args.evidence / "production-object-inputs.json").write_text(json.dumps(objects, indent=2) + "\n")
    run("fixture-configure", configure + ["-DCSE_BRIDGE_FIXTURE_LOCAL_SEAMS=ON"])
    run("fixture-build", ["cmake", "--build", args.build, "--target", "SDL3-shared", "--parallel", "2"])
    fixture = Path((args.build / ("provider-path-" + args.config + ".txt")).read_text().strip())
    assert fixture != product and sha(product) == product_sha
    fixture_symbols = exports(fixture)
    prefix = "" if os.name == "nt" else "_"
    expected_seams = {prefix + "CseSdlFixture_InitLocalVideo", prefix + "CseSdlFixture_QuitLocalVideo"}
    assert fixture_symbols - public == expected_seams and not public - fixture_symbols
    receipt["fixture"] = dict(path=str(fixture), bytes=fixture.stat().st_size, sha256=sha(fixture),
        exports=sorted(fixture_symbols), added_seams=sorted(expected_seams),
        sdk_objects_unchanged=all(sha(Path(path)) == value for path, value in objects.items()))
    fixture_images = args.evidence / "fixture-images"
    fixture_images.mkdir()
    shutil.copy2(fixture, fixture_images / fixture.name)
    for mode in ["preinit", "foreign"]:
        run(mode, [args.build / ("bridge_provider_probe" + suffix), mode, fixture, product],
            {"SDL_VIDEODRIVER": "dummy", "SDL3_DYNAMIC_API": str(product)})
        log = (args.evidence / (mode + ".log")).read_text()
        start = "PREINIT_QUERY_BEGIN" if mode == "preinit" else "FOREIGN_QUERY_BEGIN"
        end = "PREINIT_QUERY_END" if mode == "preinit" else "FOREIGN_QUERY_END"
        inside = log.split("CSE_BRIDGE_PHASE:" + start + "\n", 1)[1].split("CSE_BRIDGE_PHASE:" + end, 1)[0]
        assert not inside.strip(), "Unexpected query-region output"
    receipt["production_image_unchanged_after_fixture"] = sha(product) == product_sha
    receipt["fixture_compile_commands"] = json.loads((args.build / "compile_commands.json").read_text())
    receipt["success"] = True
finally:
    stop.set()
    monitor.join()
    disk["final_free_bytes"] = shutil.disk_usage(args.build).free
    receipt["reserve_preserved"] = os.name != "nt" or disk["minimum_free_bytes"] > reserve
    save()
if not receipt["reserve_preserved"]:
    raise SystemExit("Observed 4 GiB reserve violation")
