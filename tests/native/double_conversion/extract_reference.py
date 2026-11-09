"""Pin the actual retained GetTimeMillis method and its owning MSVC FP profile."""
from pathlib import Path
import argparse
import hashlib
import json
import xml.etree.ElementTree as ET


def extract(source, manifest, output):
    raw = source.read_bytes()
    start = raw.index(b"long CSystem::GetTimeMillis()")
    body_start = raw.index(b"{", start)
    depth = 1
    end = body_start + 1
    while depth:
        depth += (raw[end] == ord("{")) - (raw[end] == ord("}"))
        end += 1
    method = raw[start:end]
    namespace = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
    project = ET.parse(manifest).getroot()
    profiles = {}
    for config in ("Debug", "Release"):
        metadata = {}
        for group in project.findall("m:ItemDefinitionGroup", namespace):
            condition = group.get("Condition", "")
            if condition and config not in condition:
                continue
            compile_settings = group.find("m:ClCompile", namespace)
            if compile_settings is not None:
                metadata.update({node.tag.rsplit("}", 1)[-1]: node.text or "" for node in compile_settings})
        profiles[config] = metadata
        expected = {"FloatingPointModel": "Precise", "ExceptionHandling": "Sync", "LanguageStandard": "stdcpp14"}
        for name, value in expected.items():
            if metadata.get(name) != value:
                raise ValueError(f"Requalify changed {config} {name}: {metadata.get(name)}")
    system_items = [item for item in project.findall(".//m:ClCompile", namespace)
                    if item.get("Include", "").replace("\\", "/") == "src/System.cpp"]
    if len(system_items) != 1 or list(system_items[0]):
        raise ValueError("Requalify changed System.cpp source-specific policy")
    output.mkdir(parents=True, exist_ok=True)
    (output / "get_time_millis.cpp").write_bytes(b'#include "reference_clock.h"\r\n' + method + b"\r\n")
    (output / "profile.json").write_text(json.dumps({
        "source": str(source), "source_sha256": hashlib.sha256(raw).hexdigest(),
        "method_sha256": hashlib.sha256(method).hexdigest(), "method": method.decode("ascii"),
        "manifest": str(manifest), "manifest_sha256": hashlib.sha256(manifest.read_bytes()).hexdigest(),
        "profiles": profiles,
        "binding": "Exact method body; controlled GetCurrentTime double; no production provider or class ABI claim",
    }, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("output", type=Path)
    arguments = parser.parse_args()
    extract(arguments.source, arguments.manifest, arguments.output)
