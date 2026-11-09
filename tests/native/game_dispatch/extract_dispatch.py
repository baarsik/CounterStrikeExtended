from pathlib import Path
import sys

source = Path(sys.argv[1]).read_bytes().decode("cp1252").replace("\r\n", "\n")
start = source.index("DISPATCHFUNCTION GetDispatch(")
helper = source.find("template<typename Function>\nstatic Function FindGameDispatch(")
if helper >= 0:
    start = helper
end = source.index("void LoadEntityDLLs(", start)
native_loader = source.find("#if !defined(_WIN32)\nstatic void LoadNativeGameLibrary(", start, end)
if native_loader >= 0:
    end = native_loader
with Path(sys.argv[2]).open("w", encoding="utf-8", newline="\n") as output:
    output.write(source[start:end])
