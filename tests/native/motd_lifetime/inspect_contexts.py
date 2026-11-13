"""Read runtime recipes and measure complete CS VGUI translation-unit layouts.

This diagnostic compiles only a private frozen source tree. No production files,
build definitions, or live runtime producers are edited.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


parser = argparse.ArgumentParser()
parser.add_argument("--ninja", required=True)
parser.add_argument("--recipe-build", required=True)
parser.add_argument("--recipe-root", required=True)
parser.add_argument("--context-root", required=True)
parser.add_argument("--output", required=True)
parser.add_argument("--html-only", action="store_true")
args = parser.parse_args()
output = Path(args.output)
output.mkdir(parents=True, exist_ok=True)
sources = tuple(("cs_client", source) for source in (
    "src/cstrike/vgui2/game_controls/clientmotd.cpp",
    "src/cstrike/vgui2/controls/HTML.cpp",
    "src/cstrike/vgui2/controls/Frame.cpp",
    "src/cstrike/vgui2/controls/Panel.cpp",
    "src/cstrike/vgui2/controls/EditablePanel.cpp",
    "src/cstrike/vgui2/controls/BuildGroup.cpp",
))
if args.html_only:
    sources = (("cs_client", "src/cstrike/vgui2/controls/HTML.cpp"),
               ("hw_engine", "src/vgui2/controls/HTML.cpp"),
               ("gameui", "src/vgui2/controls/HTML.cpp"))
receipt = []
for target, relative in sources:
    name = target + "-" + Path(relative).stem
    object_target = f"targets/{target}/CMakeFiles/{target}.dir/__/__/__/{relative}.obj"
    recipes = subprocess.check_output(
        [args.ninja, "-C", args.recipe_build, "-t", "commands", object_target], text=True
    )
    suffix = relative.replace("/", "\\")
    candidates = [line for line in recipes.splitlines() if "cl.exe" in line and line.rstrip().endswith(suffix)]
    if len(candidates) != 1:
        raise RuntimeError(f"Expected one compiler recipe for {relative}: {len(candidates)}")
    original = candidates[0]
    command = original.replace(args.recipe_root, str(Path(args.context_root)))
    object_file = output / f"{name}.obj"
    pdb_file = output / f"{name}.pdb"
    command, object_count = re.subn(r'/Fo(?:"[^"]*"|\S+)', lambda _: f'/Fo"{object_file}"', command)
    command, pdb_count = re.subn(r'/Fd(?:"[^"]*"|\S+)', lambda _: f'/Fd"{pdb_file}"', command)
    if object_count != 1 or pdb_count != 1:
        raise RuntimeError(f"Could not redirect outputs for {relative}")
    command += " /d1reportAllClassLayout"
    (output / f"{name}-original-command.txt").write_text(original + "\n")
    (output / f"{name}-isolated-command.txt").write_text(command + "\n")
    result = subprocess.run(command, cwd=output, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (output / f"{name}-compile.log").write_text(result.stdout)
    if result.returncode != 0 or not object_file.is_file():
        raise RuntimeError(f"Complete production translation unit did not compile: {relative}")
    source = Path(args.context_root) / relative
    receipt.append({"target": target, "source": relative,
        "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "exit_code": result.returncode, "object_sha256": hashlib.sha256(object_file.read_bytes()).hexdigest()})
    print(f"motd_context_measured={name}", flush=True)
(output / "context-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
