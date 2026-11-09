from pathlib import Path
import re
import sys


def function(source, name):
    match = re.search(r"^([^\n]*\b" + re.escape(name) + r"\s*\([^;]*?\))\s*(?::[^\n]*\n)?\s*\{", source, re.M)
    if not match:
        raise ValueError(f"Missing production function {name}")
    opening = source.index("{", match.start())
    cursor = opening + 1
    depth = 1
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
    sdk, selected, output = (Path(value).resolve() for value in sys.argv[1:])
    director = (selected / "HLTV/common/DirectorCmd.cpp").read_text()
    protocol = (sdk / "HLTV/common/common.h").read_text()
    output.joinpath("actual_protocol_constants.inc").write_text(
        re.search(r"^#define svc_director\s+.*$", protocol, re.M).group() + "\n")
    prefix = ""
    if "namespace {" in director:
        prefix = director[director.index("namespace {"):director.index('extern "C" const char* DRC_CMD_NAME')]
    names = ("SetSoundData", "SetMessageData", "SetBannerData", "SetStuffTextData", "ReadFromStream",
             "WriteToStream", "Clear", "DirectorCmd", "~DirectorCmd")
    prefix = "#define CSE_HAS_DIRECTOR_SIZE_GUARD " + str(int("DirectorPayloadSize(" in director)) + "\n" + prefix
    output.joinpath("actual_director.inc").write_text(prefix + "\n".join(
        function(director, "DirectorCmd::" + name) for name in names))
    bits = (selected / "HLTV/common/BitBuffer.cpp").read_text()
    output.joinpath("actual_ctor_impl.inc").write_text(
        f'#include "{(selected / "HLTV/common/BitBuffer.cpp").as_posix()}"\n')
    # Compile the entire actual implementation, including the constructors and virtual destructor.
    output.joinpath("actual_bitbuffer.inc").write_text(bits)
    info = selected / "HLTV/common/InfoString.cpp"
    output.joinpath("actual_info.inc").write_text("#define CSE_HAS_INFO_SIZE_GUARD "
        + str(int("InfoStringCapacity(" in info.read_text())) + f'\n#include "{info.as_posix()}"\n')
    md5 = (selected / "HLTV/common/md5.cpp").read_text()
    output.joinpath("actual_md5_context.inc").write_text(typedef(md5, "MD5Context_t"))
    output.joinpath("actual_md5_key.inc").write_text(function(md5, "MD5_GetCDKeyHash"))
    bsp = (selected / "HLTV/Core/BSPModel.cpp").read_text()
    hull = "#define CSE_HAS_HULL_INDEX_GUARD " + str(int("static int BSPPlaneIndex(" in bsp)) + "\n"
    if "static int BSPPlaneIndex(" in bsp:
        hull += function(bsp, "BSPPlaneIndex")
    hull += function(bsp, "BSPModel::MakeHull0")
    output.joinpath("actual_hull.inc").write_text(hull)
    gl_model = (sdk / "engine/gl_model.h").read_text()
    bspfile = (sdk / "utils/common/bspfile.h").read_text()
    types = '#include "engine/mathlib.h"\n'
    for name in ("mplane_t", "mnode_t", "hull_t"):
        if name == "hull_t":
            types += typedef(bspfile, "dclipnode_t")
        types += typedef(gl_model, name)
    output.joinpath("actual_bsp_types.inc").write_text(types)


if __name__ == "__main__":
    main()
