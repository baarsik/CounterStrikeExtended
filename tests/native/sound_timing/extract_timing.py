from pathlib import Path
import sys

source = Path(sys.argv[1]).read_bytes().decode("cp1252").replace("\r\n", "\n")
start = source.index("int soundtime; // sample PAIRS")
end = source.index("constexpr int SOUND_CONFIG_PATH_BYTES", start)
Path(sys.argv[2]).write_text(source[start:end], encoding="utf-8")
