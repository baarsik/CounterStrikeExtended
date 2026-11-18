from pathlib import Path
import sys
root, output = map(Path, sys.argv[1:])
source = (root / 'src/engine/sys_getmodes.cpp').read_text()
start = source.index('void CVideoMode_OpenGL::ChangeDisplaySettingsToFullscreen()')
end = source.index('\nvoid CVideoMode_Common::DrawStartupGraphic', start)
output.write_text(source[start:end])

fallback = (root / 'src/engine/gl_vidnt.cpp').read_text()
start = fallback.index('        requested.w = wide;')
end = fallback.index('        Con_DPrintf("Setting closest display mode:', start)
(output.parent / 'render_mode.inc').write_text(fallback[start:end])
