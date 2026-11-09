"""Compile complete production boundaries and exact production type declarations, without rewriting their C++."""

import json
import re
import sys
from pathlib import Path


def function(source, name):
    match = re.search(r"^(?:static )?(?:int|void) " + name + r"\([^\n]*\)\s*\{", source, re.M)
    if not match:
        raise ValueError(f"Missing production function {name}")
    opening = source.index("{", match.start())
    depth = 1
    cursor = opening + 1
    while depth:
        if source[cursor] == "{":
            depth += 1
        elif source[cursor] == "}":
            depth -= 1
        cursor += 1
    return source[match.start():cursor] + "\n"


def typedef(source, name):
    end = source.index("} " + name + ";") + len("} " + name + ";")
    start = source.rfind("typedef struct", 0, end)
    return source[start:end] + "\n"


def main():
    engine, selected, output = (Path(value).resolve() for value in sys.argv[1:])
    sources = {name: (selected / name).read_text(encoding="ascii") for name in
               ("r_studio.cpp", "gl_mesh.cpp", "d_edge.cpp")}
    model = (engine / "gl_model.h").read_text(encoding="ascii")
    types = '#include "mathlib.h"\n'
    types += f'#include "{(engine.parent / "common/const.h").as_posix()}"\n'
    types += '#include "modelgen.h"\n'
    for constant in ("MAX_SKINS", "MAXALIASFRAMES"):
        types += re.search(r"^#define " + constant + r" .*$", model, re.M).group() + "\n"
    types += typedef(model, "maliasframedesc_t") + typedef(model, "aliashdr_t")
    types += typedef((engine / "d_iface.h").read_text(encoding="ascii"), "finalvert_t")
    common_model = (engine.parent / "common/com_model.h").read_text(encoding="latin1")
    types += typedef(common_model, "auxvert_t")
    studio = (engine / "studio.h").read_text(encoding="latin1")
    types += re.search(r"^#define MAXSTUDIOVERTS\s+.*$", studio, re.M).group() + "\n"
    output.joinpath("actual_types.inc").write_text(types)
    output.joinpath("selected_progs.h").write_text(f'#include "{(selected / "progs.h").as_posix()}"\n')
    output.joinpath("actual_studio.inc").write_text(function(sources["r_studio.cpp"], "R_StudioVertBuffer"))
    mesh = sources["gl_mesh.cpp"]
    has_offset_helper = "static int GL_AliasDataOffset(" in mesh
    alias = "#define CSE_HAS_ALIAS_OFFSET_HELPER " + str(int(has_offset_helper)) + "\n"
    if has_offset_helper:
        alias += function(mesh, "GL_AliasDataOffset")
    alias += function(mesh, "GL_MakeAliasModelDisplayLists")
    output.joinpath("actual_alias.inc").write_text(alias)
    edge = sources["d_edge.cpp"]
    if "static int D_SurfaceFlatColorIndex(" in edge:
        color = function(edge, "D_SurfaceFlatColorIndex")
    else:
        # The frozen implementation inlines this argument. Preserve the expression byte for byte.
        expression = re.search(r"hlRGB\( flatcolors, (\( \(int\)s->data >> 4 \) % 255) \)", edge).group(1)
        color = ("static int D_SurfaceFlatColorIndex(const void* data) {\n"
                 "    struct Surface { const void* data; } surface{data};\n"
                 "    const auto* s = &surface;\n    return " + expression + ";\n}\n")
    output.joinpath("actual_flatcolor.inc").write_text(color)
    output.joinpath("extraction-inputs.json").write_text(json.dumps({
        "selected_sources": str(selected), "production_types": str(engine),
        "boundaries": ["progs.h (whole header)", "R_StudioVertBuffer (whole function)",
                       "GL_MakeAliasModelDisplayLists (whole function)",
                       "GL_AliasDataOffset (whole helper when present)",
                       "D_SurfaceFlatColorIndex (whole helper or frozen inline expression)"]}, indent=2) + "\n")


if __name__ == "__main__":
    main()
