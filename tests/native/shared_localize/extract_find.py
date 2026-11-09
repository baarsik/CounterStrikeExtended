from pathlib import Path
import sys

source = Path(sys.argv[1]).read_bytes().decode("cp1252").replace("\r\n", "\n")
start = source.index("wchar_t* SharedFindString(")
end = source.index("\n#endif", start)
with Path(sys.argv[2]).open("w", encoding="utf-8", newline="\n") as output:
    output.write(source[start:end] + "\n")
