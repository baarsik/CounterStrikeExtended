import argparse
import json
from pathlib import Path
import subprocess
import sys
import shlex
from case_list import CASES

parser = argparse.ArgumentParser()
parser.add_argument("--repo", type=Path, required=True)
parser.add_argument("--out", type=Path, required=True)
parser.add_argument("--database", type=Path, required=True)
parser.add_argument("--owner", choices=["common", "cs"], required=True)
parser.add_argument("--test", default="copy")
parser.add_argument("--sanitize", action="store_true")
args = parser.parse_args()
here = Path(__file__).resolve().parent
subprocess.run([sys.executable, str(here / "prepare_fixture.py"), "--repo", str(args.repo), "--out", str(args.out),
                "--database", str(args.database), "--owner", args.owner], check=True)
context = json.loads((args.out / "owning-context.json").read_text())
vpanel_source = args.repo / "src/vgui2/src/VPanel.cpp"
vpanel_entry = next(e for e in json.loads(args.database.read_text()) if e["file"] == str(vpanel_source))
vpanel_flags = []
skip = False
for token in shlex.split(vpanel_entry["command"]):
    if skip:
        skip = False
        continue
    if token == "-o":
        skip = True
    elif token not in ("-c", str(vpanel_source)):
        vpanel_flags.append(token)
vpanel_commands = []
for source, name in [(vpanel_source, "actual_vpanel.o"), (here / "vpanel_binding.cpp", "vpanel_binding.o")]:
    vpanel_command = vpanel_flags + ["-c", str(source), "-o", str(args.out / name)]
    if args.sanitize:
        vpanel_command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    result = subprocess.run(vpanel_command, capture_output=True, cwd=vpanel_entry["directory"])
    (args.out / (name + ".log")).write_bytes(result.stdout + result.stderr)
    vpanel_commands.append({"command": vpanel_command, "exit": result.returncode})
    if result.returncode:
        print(result.stderr.decode()[-5000:])
        sys.exit(result.returncode)
(args.out / "vpanel-commands.json").write_text(json.dumps(vpanel_commands, indent=2) + "\n")
command = context["flags"] + ["-I" + str(args.out), "-I" + str(args.repo / "src/vgui2/src"),
                               "-I" + str(here), str(here / "widget_tests.cpp"),
                               str(here / "unexpected_binding.cpp"),
                               "-I" + str(args.repo / "src/common"),
                               str(args.out / "actual_clipboard_helper.cpp"),
                               str(args.out / "actual_vpanel.o"), str(args.out / "vpanel_binding.o"),
                               "-Wl,-dead_strip", "-o", str(args.out / "widget-tests")]
if args.sanitize:
    command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
result = subprocess.run(command, capture_output=True, cwd=context["entry"]["directory"])
(args.out / "build.log").write_bytes(result.stdout + result.stderr)
(args.out / "build-command.json").write_text(json.dumps({"command": command, "exit": result.returncode}, indent=2) + "\n")
if result.returncode:
    print(result.stderr.decode(errors="replace")[-11000:])
    sys.exit(result.returncode)
rows = []
for test in CASES if args.test == "all" else [args.test]:
    result = subprocess.run([str(args.out / "widget-tests"), test], capture_output=True)
    (args.out / (test + "-run.log")).write_bytes(result.stdout + result.stderr)
    rows.append({"test": test, "exit": result.returncode})
    if result.returncode:
        print(result.stdout.decode() + result.stderr.decode())
(args.out / "results.json").write_text(json.dumps(rows, indent=2) + "\n")
print(args.owner, len(rows), "actual-widget controls; failures", sum(r["exit"] != 0 for r in rows))
sys.exit(int(any(r["exit"] for r in rows)))
