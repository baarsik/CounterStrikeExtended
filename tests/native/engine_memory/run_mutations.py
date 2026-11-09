"""Causal controls on isolated copies; never edit selected production source."""
import argparse
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--art", type=Path, required=True)
    parser.add_argument("--stage", type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]
    tests = Path(__file__).resolve().parent
    args.art.mkdir(parents=True, exist_ok=True)
    header = (repo / "GoldSrc-SDK/common/NativeEngineMemory.h").read_text()
    mutations = [
        ("wrong-sdk-names", "query_tests", "header", "::sysconf(_SC_PHYS_PAGES)", "::sysconf(85)"),
        ("truncated-page-count", "query_tests", "header", "ClassifyPhysicalMemory(pages, pageBytes)",
         "ClassifyPhysicalMemory(static_cast<std::int32_t>(pages), pageBytes)"),
        ("missing-dedicated-policy", "pure_tests", "header", "bytes = kDedicatedHeapBytes;", "bytes = bytes;"),
        ("unknown-is-minimum", "pure_tests", "header", "bytes = kMaximumHeapBytes;", "bytes = kMinimumHeapBytes;"),
        ("wrong-cli-scale", "pure_tests", "header", "bytes = kibibytes * kBytesPerKibibyte;",
         "bytes = kibibytes * (kBytesPerKibibyte - 1);"),
        ("low-ram-error-skipped", "pure_tests", "header", "if (!unknown && physical.totalBytes < kMinimumPhysicalBytes)",
         "if (false && physical.totalBytes < kMinimumPhysicalBytes)"),
        ("early-minmemory-query", "actual_body_tests", "body", "memory::HeapPlan plan{};",
         'COM_CheckParm("-minmemory");\n    memory::HeapPlan plan{};'),
        ("returning-error-falls-through", "actual_body_tests", "body",
         'Sys_Error("Invalid -heapsize: expected nonnegative KiB within signed32 bytes\\n");\n            return;',
         'Sys_Error("Invalid -heapsize: expected nonnegative KiB within signed32 bytes\\n");')
    ]
    results = []
    for name, fixture, target, before, after in mutations:
        work = args.art / name
        work.mkdir(exist_ok=True)
        (work / "NativeEngineMemory.h").write_text(header)
        for path in args.stage.glob("*.inc"):
            (work / path.name).write_bytes(path.read_bytes())
        paths = [work / "NativeEngineMemory.h"] if target == "header" else list(work.glob("*-Sys_InitMemory.inc"))
        replacements = 0
        for path in paths:
            source = path.read_text()
            replacements += source.count(before)
            path.write_text(source.replace(before, after))
        assert replacements > 0, name
        exe = work / "control"
        command = ["/usr/bin/clang++", "-std=c++14", "-O0", "-fexceptions", "-I" + str(work),
            "-I" + str(repo / "GoldSrc-SDK/common"), "-I" + str(repo / "GoldSrc-SDK/engine"),
            "-I" + str(repo / "GoldSrc-SDK/public"), "-I" + str(tests), str(tests / (fixture + ".cpp")), "-o", str(exe)]
        compiled = subprocess.run(command, capture_output=True, timeout=120)
        (work / "compile.log").write_bytes(compiled.stdout + compiled.stderr)
        assert compiled.returncode == 0, (name, compiled.stderr.decode())
        observed = subprocess.run([str(exe)], capture_output=True, timeout=120)
        (work / "run.log").write_bytes(observed.stdout + observed.stderr)
        assert observed.returncode != 0, name + " escaped behavioral gate"
        results.append({"name": name, "command": command, "replacements": replacements,
            "compile_exit": compiled.returncode, "run_exit": observed.returncode,
            "failure": observed.stderr.decode(errors="replace").strip()})
    (args.art / "RESULTS.json").write_text(json.dumps(results, indent=2) + "\n")
    print("PASS: all eight independently compiled production mutations rejected by behavior")


if __name__ == "__main__":
    main()
