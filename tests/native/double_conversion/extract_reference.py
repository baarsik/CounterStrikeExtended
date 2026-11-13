"""Pin the actual retained GetTimeMillis method and its owning MSVC FP profile."""
from pathlib import Path
import argparse
import hashlib
import json
import sys


def extract(source, root, output):
    raw = source.read_bytes()
    start = raw.index(b"long CSystem::GetTimeMillis()")
    body_start = raw.index(b"{", start)
    depth = 1
    end = body_start + 1
    while depth:
        depth += (raw[end] == ord("{")) - (raw[end] == ord("}"))
        end += 1
    method = raw[start:end]
    sys.path.insert(0, str(root / "tools"))
    from cmake_inventory import query_target
    profiles = {}
    for config in ("Debug", "Release"):
        target = query_target(root, "vgui2", configuration=config, target_os="windows", architecture="x86")
        selected = [item for item in target["sources"]
                    if Path(item["path"]) == root / "src/vgui2/src/System.cpp"]
        if len(selected) != 1:
            raise ValueError("Owning CMake target must select exactly one System.cpp")
        profile = selected[0]
        options = profile["compile_options"]
        floating = next((value for value in reversed(options) if value.startswith("/fp:")), None)
        exceptions = next((value for value in reversed(options) if value.startswith("/EH")), None)
        standard = str(target["properties"].get("CXX_STANDARD"))
        if (floating, exceptions, standard) != ("/fp:precise", "/EHsc", "14"):
            raise ValueError(f"Requalify changed {config} System.cpp policy: {floating}, {exceptions}, C++{standard}")
        profiles[config] = {"source": profile, "properties": target["properties"]}
    output.mkdir(parents=True, exist_ok=True)
    (output / "get_time_millis.cpp").write_bytes(b'#include "reference_clock.h"\r\n' + method + b"\r\n")
    (output / "profile.json").write_text(json.dumps({
        "source": str(source), "source_sha256": hashlib.sha256(raw).hexdigest(),
        "method_sha256": hashlib.sha256(method).hexdigest(), "method": method.decode("ascii"),
        "cmake_root": str(root), "target": "vgui2", "target_os": "windows", "architecture": "x86",
        "profiles": profiles,
        "binding": "Exact method body; controlled GetCurrentTime double; no production provider or class ABI claim",
    }, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("root", type=lambda value: Path(value).resolve())
    parser.add_argument("output", type=Path)
    arguments = parser.parse_args()
    extract(arguments.source, arguments.root, arguments.output)
