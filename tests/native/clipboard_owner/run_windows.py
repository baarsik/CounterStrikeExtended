"""VM1 console-only pure owner fixture; no OS clipboard/window/provider calls."""
import hashlib
import json
import pathlib
import shutil
import subprocess


def main():
    shared = pathlib.Path(__file__).resolve().parent
    source = shared / "windows-source"
    manifest = json.loads((shared / "windows-source.json").read_text())
    work = pathlib.Path(r"C:\CI\CSE-ClipboardOwnerV3-20261109")
    work.mkdir(exist_ok=False)
    staged = work / "source"
    shutil.copytree(source, staged)
    for relative, expected in manifest.items():
        assert hashlib.sha256((staged / relative).read_bytes()).hexdigest() == expected
    results = []
    for architecture in ["x86", "x64", "arm64"]:
        compiler = pathlib.Path(r"C:\BuildTools\2026\VC\Tools\MSVC\14.51.36231\bin\Hostarm64") / architecture / "cl.exe"
        compiler_hash = hashlib.sha256(compiler.read_bytes()).hexdigest()
        for profile, options in [("Release", ["/O2", "/MT"]), ("Debug", ["/Od", "/Zi", "/MTd"])]:
            case = architecture + "-" + profile
            directory = work / case
            directory.mkdir()
            command = [str(compiler), "/nologo", "/std:c++14", "/EHsc", "/fp:precise", "/W4", "/WX",
                       "/Zc:wchar_t", *options, "/I" + str(staged), str(staged / "owner_tests.cpp"),
                       str(staged / "NativeClipboard.cpp"), str(staged / "NativeClipboardOwner.cpp"),
                       "/Fe:" + str(directory / "owner.exe")]
            shell = 'call "C:\\BuildTools\\2026\\Common7\\Tools\\VsDevCmd.bat" -no_logo -host_arch=arm64 -arch='
            shell += architecture + " >nul && " + subprocess.list2cmdline(command)
            batch = directory / "build.cmd"
            batch.write_text("@echo off\n" + shell + "\n")
            result = subprocess.run(["cmd.exe", "/d", "/c", str(batch)], cwd=directory,
                                    capture_output=True, text=True)
            (shared / (case + "-compile.log")).write_text(result.stdout + result.stderr)
            row = {"case": case, "compiler": str(compiler), "compiler_sha256": compiler_hash,
                   "command": command, "setup_and_command": shell, "compile_exit": result.returncode}
            if result.returncode == 0:
                executed = subprocess.run([str(directory / "owner.exe")], cwd=directory,
                                          capture_output=True, text=True, timeout=30)
                (shared / (case + "-run.log")).write_text(executed.stdout + executed.stderr)
                row["run_exit"] = executed.returncode
                row["exe_sha256"] = hashlib.sha256((directory / "owner.exe").read_bytes()).hexdigest()
            results.append(row)
            (shared / "windows-matrix.json").write_text(json.dumps(results, indent=2) + "\n")
    assert len(results) == 6 and all(x["compile_exit"] == 0 and x.get("run_exit") == 0 for x in results), results
    print("PASS: actual MSVC x86/x64/ARM64 Release/Debug pure owner fixture; no clipboard APIs")


if __name__ == "__main__":
    main()
