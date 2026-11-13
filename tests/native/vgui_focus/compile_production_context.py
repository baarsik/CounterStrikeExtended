"""Compile complete Panel translation units using retained runtime Ninja recipes.

Run inside a matching MSVC environment. Recipe discovery is read-only; all inputs
and compiler outputs are redirected to private frozen source/output directories.
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
parser.add_argument("--candidate-root", required=True)
parser.add_argument("--output", required=True)
parser.add_argument("--expect-cs-type-error", action="store_true")
args = parser.parse_args()
output = Path(args.output)
output.mkdir(parents=True, exist_ok=True)
contexts = (
    ("cs_client", "src/cstrike/vgui2/controls/Panel.cpp"),
    ("hw_engine", "src/vgui2/controls/Panel.cpp"),
    ("gameui", "src/vgui2/controls/Panel.cpp"),
)
receipt = []
for target, relative in contexts:
    if args.expect_cs_type_error and target != "cs_client":
        continue
    object_target = f"targets/{target}/CMakeFiles/{target}.dir/__/__/__/{relative}.obj"
    recipes = subprocess.check_output(
        [args.ninja, "-C", args.recipe_build, "-t", "commands", object_target], text=True
    )
    source_suffix = relative.replace("/", "\\")
    candidates = [
        line for line in recipes.splitlines()
        if "cl.exe" in line and line.rstrip().endswith(source_suffix)
    ]
    if len(candidates) != 1:
        raise RuntimeError(f"Expected exactly one compiler recipe for {target}: {len(candidates)}")
    original = candidates[0]
    context_root = str(Path(args.context_root))
    command = original.replace(args.recipe_root, context_root)
    context_input = str(Path(context_root) / Path(relative))
    candidate_input = str(Path(args.candidate_root) / Path(relative))
    if not command.endswith(context_input):
        raise RuntimeError(f"Source remapping failed for {target}")
    command = command[:-len(context_input)] + candidate_input
    object_file = output / f"{target}-Panel.obj"
    pdb_file = output / f"{target}-Panel.pdb"
    command, object_count = re.subn(r'/Fo(?:"[^"]*"|\S+)', lambda _: f'/Fo"{object_file}"', command)
    command, pdb_count = re.subn(r'/Fd(?:"[^"]*"|\S+)', lambda _: f'/Fd"{pdb_file}"', command)
    if object_count != 1 or pdb_count != 1:
        raise RuntimeError(f"Output remapping failed for {target}")
    (output / f"{target}-original-command.txt").write_text(original + "\n")
    (output / f"{target}-isolated-command.txt").write_text(command + "\n")
    result = subprocess.run(command, cwd=output, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (output / f"{target}-compile.log").write_text(result.stdout)
    expected_failure = args.expect_cs_type_error and target == "cs_client"
    if expected_failure:
        if result.returncode == 0 or "identifier 'uint64'" not in result.stdout:
            raise RuntimeError(f"Did not reproduce the expected uint64 declaring-context failure: {target}")
    elif result.returncode != 0 or not object_file.is_file():
        raise RuntimeError(f"Complete production translation unit did not compile: {target}")
    receipt.append({
        "target": target,
        "source": candidate_input,
        "source_sha256": hashlib.sha256(Path(candidate_input).read_bytes()).hexdigest(),
        "exit_code": result.returncode,
        "expected_type_error": expected_failure,
        "object_sha256": hashlib.sha256(object_file.read_bytes()).hexdigest() if object_file.is_file() else None,
    })
    print(f"production_context_verified={target} expected_type_error={int(expected_failure)}", flush=True)
(output / "production-context-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
