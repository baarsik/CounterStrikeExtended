from pathlib import Path
import re
import sys

source = Path(sys.argv[1]).read_bytes().decode('cp1252')
output = Path(sys.argv[2])
match = re.search(r'^static int ParseNativeMp3Track\([^\n]*\)\s*\{', source, re.M)
if not match:
    raise SystemExit('Missing actual native CD track parser')
end = source.index('\n}', match.end()) + len('\n}')
windows = re.search(r'int track = (atoi\( Cmd_Argv\( 2 \) \) \+ 1);', source)
if not windows:
    raise SystemExit('Missing retained Windows MP3 track expression')
expression = windows[1].replace('Cmd_Argv( 2 )', 'argument')
output.write_text(source[match.start():end] + '\n#if defined(_WIN32)\n'
                  + 'static int ActualWindowsTrack(const char* argument) { return ' + expression + '; }\n#endif\n')
