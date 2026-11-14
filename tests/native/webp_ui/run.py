#!/usr/bin/env python3
"""Run production UI image adapters against small, distinct-color fixtures."""
from pathlib import Path
import argparse
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--codec-library', type=Path, help='Built cse_image_codec static library (optional)')
parser.add_argument('--webp-library', type=Path, help='Pinned static libwebp library (optional)')
parser.add_argument('--banner-only', action='store_true')
parser.add_argument('--font-only', action='store_true')
parser.add_argument('--baseline-sdl', action='store_true', help='Demonstrate the SDL regression against HEAD')
parser.add_argument('--sanitize', action='store_true', help='Enable address and undefined-behavior sanitizers')
args = parser.parse_args()
compiler = ['clang++', '-std=c++14', '-Wall', '-Wextra']
if args.sanitize:
    compiler += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
root = Path(__file__).resolve().parents[3]
fixture = Path(__file__).resolve().parent
vgui = root / 'src/utils/vgui/lib-src/vgui'
def function(source, name):
    start = source.index(name)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'
with tempfile.TemporaryDirectory(prefix='cse-webp-ui-') as temporary:
    client = (root / 'src/game_shared/vgui_loadtga.cpp').read_text(errors='surrogateescape')
    Path(temporary, 'client_functions.inc').write_text(client[client.index('class MemoryInputStream'):])
    command = compiler + ['-I' + str(fixture / 'compat'),
               '-I' + temporary,
               '-I' + str(vgui / 'include'), '-I' + str(vgui / 'common'), '-I' + str(root / 'src/common'),
               str(fixture / 'loader_tests.cpp'), str(vgui / 'BitmapTGA.cpp'),
               str(vgui / 'FileInputStream.cpp'), str(vgui / 'DataInputStream.cpp'),
               str(vgui / 'common/fileimage.cpp')]
    codec = []
    if args.codec_library:
        codec = [str(args.codec_library), str(args.webp_library)]
    elif (root / 'src/common/image_codec.cpp').exists():
        library = args.webp_library or root / 'build/webp-codec-debug/lib/Debug/webp/md/libcse_webp_md.a'
        webp_source = root / 'build/webp-codec-debug/dependencies/WebP/source/libwebp-1.6.0/src'
        if not library.is_file() or not webp_source.is_dir():
            parser.error('Build tests/native/webp_codec in build/webp-codec-debug first; see README.md')
        codec = [str(root / 'src/common/image_codec.cpp'), '-I' + str(webp_source), str(library)]
    output = Path(temporary) / 'loader_tests'
    if not args.banner_only and not args.font_only:
        subprocess.run(command + codec + ['-o', str(output)], check=True)
        subprocess.run([str(output), str(fixture / 'fixture.webp'), temporary], check=True)
        # Copy the unmodified production TU so only the filesystem writer's
        # include boundary is substituted; the complete loader stays real.
        Path(temporary, 'vgui2_fileimage.cpp').write_bytes((root / 'src/vgui2/src/fileimage.cpp').read_bytes())
        subprocess.run(compiler + ['-I' + str(fixture / 'compat'),
                        '-I' + str(root / 'src/vgui2/src'), '-I' + str(root / 'src/common'),
                        str(fixture / 'vgui2_fileimage_tests.cpp'), str(Path(temporary, 'vgui2_fileimage.cpp'))]
                       + codec + ['-o', str(output)], check=True)
        subprocess.run([str(output), str(fixture / 'fixture.webp')], check=True)
        sdl = (root / 'src/vgui2/src/SurfaceSDL.cpp').read_text()
        if args.baseline_sdl:
            sdl = subprocess.check_output(['git', 'show', 'HEAD:src/vgui2/src/SurfaceSDL.cpp'], cwd=root).decode()
        win = (root / 'src/vgui2/src/Surface.cpp').read_text(errors='surrogateescape')
        functions = function(sdl, 'bool DecodeIndexedBitmap(')
        functions += function(sdl, 'void CSDLSurface::DrawSetTextureFile(')
        for name in ['bool CWin32Surface::UploadImagePixels(', 'bool CWin32Surface::LoadWebP(',
                     'bool CWin32Surface::LoadTGA(']:
            functions += function(win, name)
        Path(temporary, 'surface_functions.inc').write_text(functions)
        subprocess.run(compiler + ['-I' + temporary,
                        '-I' + str(root / 'src/common'), str(fixture / 'surface_tests.cpp')]
                       + codec + ['-o', str(output)], check=True)
        subprocess.run([str(output), str(fixture / 'fixture.webp'), temporary], check=True)
        logo = (root / 'src/GameUI/LogoFile.cpp').read_text(errors='surrogateescape')
        functions = function(logo, 'int fgetLittleShort(')
        functions += function(logo, 'bool LoadTGA( const char* szFilename, byte* buffer, int bufferSize, '
                             'int* width, int* height, int errFail )\n{')
        Path(temporary, 'logo_functions.inc').write_text(functions)
        subprocess.run(compiler + ['-I' + temporary,
                        '-I' + str(root / 'src/common'), str(fixture / 'logo_tests.cpp')]
                       + codec + ['-o', str(output)], check=True)
        subprocess.run([str(output), str(fixture / 'fixture.webp'), temporary], check=True)
    if not args.banner_only:
        scheme = (root / 'src/game_shared/vgui_SchemeManager.cpp').read_text(errors='surrogateescape')
        functions = ''
        if 'unsigned char* LoadBitmapFontFile(' in scheme:
            functions += function(scheme, 'unsigned char* LoadBitmapFontFile(')
        functions += '''std::vector<unsigned char> LoadSchemeFont(int m_xRes, const char* name) {
            struct Scheme { const char* schemeName; };
            const Scheme m_pSchemeList[] = {{name}};
            const int i = 0;
            char fontFilename[128]{};
            int fontFileLength = -1;
            unsigned char* pFontData = nullptr;
        '''
        functions += function(scheme, 'if(g_CV_BitmapFonts && g_CV_BitmapFonts->value)')
        functions += '''std::vector<unsigned char> bytes;
            if (pFontData) {
                bytes.assign(pFontData, pFontData + fontFileLength);
                gEngfuncs.COM_FreeFile(pFontData);
            }
            return bytes;
        }'''
        Path(temporary, 'font_lookup_functions.inc').write_text(functions)
        Path(temporary, 'gfx/vgui/fonts').mkdir(parents=True)
        subprocess.run(compiler + ['-Wno-deprecated-declarations', '-I' + temporary, '-I' + str(vgui / 'common'),
                        '-I' + str(root / 'src/common'), str(fixture / 'font_lookup_tests.cpp'),
                        str(vgui / 'common/fileimage.cpp')]
                       + codec + ['-o', str(output)], check=True)
        subprocess.run([str(output), str(fixture / 'fixture.webp')], cwd=temporary, check=True)
    if args.font_only:
        raise SystemExit(0)
    source = (root / 'src/HLTV/Proxy/Proxy.cpp').read_text()
    start = source.index('void Proxy::CMD_BannerFile(')
    end = source.index('\nvoid Proxy::CMD_CheeringThreshold', start)
    Path(temporary, 'banner_function.inc').write_text(source[start:end])
    subprocess.run(compiler + ['-I' + temporary,
                    '-I' + str(root / 'src/common'), str(fixture / 'banner_tests.cpp')]
                   + codec + ['-o', str(output)], check=True)
    subprocess.run([str(output), str(fixture / 'fixture.webp'), temporary], check=True)
