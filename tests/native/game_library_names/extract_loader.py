from pathlib import Path
import sys

source = Path(sys.argv[1]).read_bytes().decode("cp1252").replace("\r\n", "\n")
start = source.find("#if !defined(_WIN32)\nstatic void LoadNativeGameLibrary(")
if start < 0:
    start = source.index("void LoadEntityDLLs(")
end = source.index("\n\tgNewDLLFunctions.pfnCvarValue2", start)
Path(sys.argv[2]).write_text(source[start:end] + "\n}\n", encoding="utf-8")
