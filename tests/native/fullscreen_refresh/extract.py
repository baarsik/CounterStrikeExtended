from pathlib import Path
import sys
root, output = map(Path, sys.argv[1:])
source = (root / 'GoldSrc-SDK/engine/sys_getmodes.cpp').read_text()
start = source.index('void CVideoMode_OpenGL::ChangeDisplaySettingsToFullscreen()')
end = source.index('\nvoid CVideoMode_Common::DrawStartupGraphic', start)
output.write_text(source[start:end])
