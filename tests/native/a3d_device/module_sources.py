"""Evaluate the retained A3D manifest, including generated geometry sections."""
from pathlib import Path
import json
import re
import sys

sys.dont_write_bytecode = True
root = Path(sys.argv[1]).resolve()
source = Path(sys.argv[2]).resolve()
config = sys.argv[3]
arch = sys.argv[4]
output = Path(sys.argv[5])
sys.path.insert(0, str(root / "tools"))
import translate_msbuild as translator

project = root / "GoldSrc-SDK/projects/vs2026/a3dapi.vcxproj"
evaluated = translator.evaluate(project, config)
original = root / "GoldSrc-SDK/a3dapi"


def mapped(path):
    path = Path(path)
    return source / path.name if path.parent == original else path


def quote(value):
    return '"' + str(value).replace("\\", "/").replace('"', '\\"') + '"'


items = [item for item in evaluated["items"] if item["kind"] in ("ClCompile", "ResourceCompile")
         and item["metadata"].get("ExcludedFromBuild", "false") != "true"]
paths = []
settings = []
for item in items:
    metadata = item["metadata"]
    if "generated" in item:
        path = output.parent / Path(item["path"]).name
        path.write_text("#define " + item["generated"]["section"] + '\n#include "'
                        + str(source / "A3dGeom.cpp").replace("\\", "/") + '"\n')
    else:
        path = mapped(item["path"])
    paths.append(path)
    definitions = [v for v in metadata.get("PreprocessorDefinitions", "").split(";") if v]
    includes = [mapped(v) for v in metadata.get("AdditionalIncludeDirectories", "").split(";") if v]
    options = []
    optimization = {"Disabled": "/Od", "MaxSpeed": "/O2"}.get(metadata.get("Optimization"))
    if optimization:
        options.append(optimization)
    floating = {"Precise": "/fp:precise", "Strict": "/fp:strict", "Fast": "/fp:fast"}.get(metadata.get("FloatingPointModel"))
    if floating:
        options.append(floating)
    exceptions = {"Sync": "/EHsc", "Async": "/EHa"}.get(metadata.get("ExceptionHandling"))
    if exceptions:
        options.append(exceptions)
    if metadata.get("IntrinsicFunctions") == "true":
        options.append("/Oi")
    if arch == "x86" and metadata.get("EnableEnhancedInstructionSet") == "NoExtensions":
        options.append("/arch:IA32")
    for key, values in [("COMPILE_DEFINITIONS", definitions), ("INCLUDE_DIRECTORIES", includes),
                        ("COMPILE_OPTIONS", options if item["kind"] == "ClCompile" else [])]:
        if values:
            settings.append("set_property(SOURCE " + quote(path) + " APPEND PROPERTY " + key + " "
                            + " ".join(quote(v) for v in values) + ")")

definition = source / "a3dapi.def"
if arch != "x86":
    # Fixture-only native C export projection: preserve names, ordinals, PRIVATE.
    # Root production export-policy integration is a separate requirement.
    text = definition.read_text(encoding="cp1252")
    text = re.sub(r"(?m)^(\s*)(\w+)=_\2@\d+(\s+@\d+\s+PRIVATE)\s*$", r"\1\2\3", text)
    assert "=_" not in text
    definition = output.parent / "native-a3dapi.def"
    definition.write_text(text)
paths.append(definition)
lines = ["set(CSE_A3D_MODULE_SOURCES"] + ["  " + quote(p) for p in paths] + [")"] + settings
lines += ["set(CSE_A3D_MODULE_INCLUDE " + quote(source) + ")"]
libraries = [v for v in evaluated["definitions"]["Link"]["AdditionalDependencies"].split(";") if v]
lines += ["set(CSE_A3D_MODULE_LIBRARIES " + " ".join(quote(v) for v in libraries) + ")"]
output.write_text("\n".join(lines) + "\n")
output.with_suffix(".json").write_text(json.dumps(evaluated, indent=2) + "\n")
text = (source / "A3d3.cpp").read_text(encoding="cp1252")
first = text.index("static const CLSID s_A3dDalClassId")
(output.parent / "factory_ids.inc").write_text(text[first:text.index("CA3dClassFactory::CA3dClassFactory", first)])
