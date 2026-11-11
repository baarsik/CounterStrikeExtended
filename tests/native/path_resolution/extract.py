from pathlib import Path
import sys
source = Path(sys.argv[1]).read_text(encoding='latin1')
start = source.index('int COM_ExpandFilename( char* filename )')
end = source.index('\nvoid COM_DefaultExtension', start)
Path(sys.argv[2]).write_text(source[start:end])
