"""Extract actual filesystem consumers and the surface's language initialization statements."""
import hashlib
import json
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2])
out.mkdir(parents=True, exist_ok=True)
fs = (root / 'src/engine/filesystem.cpp').read_text()
ui = (root / 'src/engine/vgui2/BaseUISurface.cpp').read_text()


def block(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    end = opening + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


parts = []
if 'std::string FileSystem_GetInterfaceLanguage(' in fs:
    parts.append(block(fs, 'std::string FileSystem_GetInterfaceLanguage('))
parts.extend(block(fs, signature) for signature in
             ('int FileSystem_SetGameDirectory(', 'int FileSystem_AddFallbackGameDir('))
(out / 'filesystem.inc').write_text('\n\n'.join(parts) + '\n')
init = block(ui, 'void BaseUISurface::Init(')
start = init.index('SetEmbeddedPanel(')
end = init.index('m_pEmulatedMouseImage', start)
initialization = 'void BaseUISurface::InitLanguage(int embeddedPanel) {\n' + init[start:end] + '}\n'
(out / 'surface.inc').write_text(block(ui, 'void BaseUISurface::SetLanguage(') + '\n' + initialization)
dedicated_parts = []
for name, function in (('src/dedicated/native/NativeGui.cpp', 'ApplyNativeDedicatedLanguage'),
                       ('src/dedicated/sys_ded.cpp', 'ApplyWindowsDedicatedLanguage')):
    source = (root / name).read_text()
    marker = 'const std::string language = cse::interface_language::ResolveFromSystem('
    start = source.index(marker)
    end = source.index('vgui2::surface()->SetLanguage(language.c_str());', start)
    end = source.index('\n', end) + 1
    assert end < source.index('vgui2::localize()->AddFile'), 'dedicated language must precede catalogue loads'
    dedicated_parts.append('void ' + function + '() {\n' + source[start:end] + '}\n')
(out / 'dedicated.inc').write_text('\n'.join(dedicated_parts))
(out / 'source-manifest.json').write_text(json.dumps({
    name: hashlib.sha256((root / name).read_bytes()).hexdigest() for name in
    ('src/engine/filesystem.cpp', 'src/engine/vgui2/BaseUISurface.cpp',
     'src/common/interface_language.h', 'src/dedicated/native/NativeGui.cpp', 'src/dedicated/sys_ded.cpp')
}, indent=2) + '\n')
