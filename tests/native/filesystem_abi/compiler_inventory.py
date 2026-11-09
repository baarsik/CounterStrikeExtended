"""Measure actual header method types and compiler-emitted virtual call slots."""

import argparse
import json
from pathlib import Path
import re
import subprocess


parser = argparse.ArgumentParser()
parser.add_argument("--root", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--clang", default="clang++")
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
views = {"provider": "filesystem/basefilesystem.h", "public": "public/FileSystem.h",
         "launcher": "launcher/public/filesystem.h", "vgui": "vgui2/src/public/FileSystem.h"}
result = {}
for view, header in views.items():
    source = args.output / f"{view}-declarations.cpp"
    source.write_text(f'#include "{header}"\n')
    common = [args.clang, "-std=c++14", "-I", str(args.root / "GoldSrc-SDK"),
              "-I", str(args.root / "GoldSrc-SDK/public"), "-I", str(args.root / "GoldSrc-SDK/filesystem")]
    ast_text = subprocess.check_output(common + ["-Xclang", "-ast-dump=json", "-Xclang",
        "-ast-dump-filter=IFileSystem", "-fsyntax-only", str(source)], text=True)
    (args.output / f"{view}-ast.json").write_text(ast_text)
    ast = json.loads(ast_text)
    methods = [node for node in ast["inner"] if node["kind"] == "CXXMethodDecl" and node.get("virtual")][:54]
    if len(methods) != 54 or methods[-1]["name"] != "GetFileModificationTime":
        raise RuntimeError("Consumed public prefix changed")
    wrappers = f'#include "{header}"\n'
    for index, method in enumerate(methods):
        parameters = [node for node in method.get("inner", []) if node["kind"] == "ParmVarDecl"]
        for number, parameter in enumerate(parameters):
            wrappers += f'using Arg{index}_{number} = {parameter["type"]["qualType"]};\n'
        declarations = [f"Arg{index}_{number} arg{number}" for number in range(len(parameters))]
        arguments = [f"arg{number}" for number in range(len(parameters))]
        if method.get("variadic"):
            arguments += ['23', '2.5', '"probe"']
        call = 'self->' + method['name'] + '(' + ', '.join(arguments) + ')'
        wrappers += (f'extern "C" auto fs_call_{index}(IFileSystem* self'
            + (', ' + ', '.join(declarations) if declarations else '') + f') -> decltype({call}) {{ return {call}; }}\n')
    probe = args.output / f"{view}-calls.cpp"
    probe.write_text(wrappers)
    ir_path = args.output / f"{view}-calls.ll"
    subprocess.run(common + ["-S", "-emit-llvm", "-O0", "-fno-discard-value-names",
        str(probe), "-o", str(ir_path)], check=True)
    ir = ir_path.read_text()
    measured = []
    for index, method in enumerate(methods):
        block = re.search(r'define[^\n]*@fs_call_' + str(index) + r'\([^\n]*\).*?\n}', ir, re.S)
        if not block:
            raise RuntimeError(f"Missing compiler wrapper {view} {index}")
        slots = re.findall(r'getelementptr inbounds ptr, ptr %vtable, i64 (\d+)', block.group())
        if len(slots) != 1:
            raise RuntimeError(f"Ambiguous virtual call slot {view} {index}: {slots}")
        slot = int(slots[0])
        if slot != index + 2:
            raise RuntimeError(f"Unexpected Itanium prefix shape {view} {index}: {slot}")
        measured.append({"ordinal": index, "slot": slot, "name": method["name"],
            "method_type": method["type"], "variadic": method.get("variadic", False),
            "parameters": [node["type"] for node in method.get("inner", []) if node["kind"] == "ParmVarDecl"]})
    result[view] = measured
(args.output / "compiler-method-inventory.json").write_text(json.dumps(result, indent=2) + "\n")
print("filesystem_actual_itanium_slots_verified=216")
