from pathlib import Path
import re
import sys

common = Path(sys.argv[1]).read_bytes().decode("cp1252").replace("\r\n", "\n")
voice = Path(sys.argv[2]).read_bytes().decode("cp1252").replace("\r\n", "\n")
start = common.index("constexpr int WAVE_CHUNK_ID_BYTES = 4;")
end = common.index("void COM_NormalizeAngles(", start)
voice = re.sub(r'^#include[^\n]*\n', '', voice, flags=re.MULTILINE)
Path(sys.argv[3]).write_text(common[start:end] + "\n" + voice, encoding="utf-8")
