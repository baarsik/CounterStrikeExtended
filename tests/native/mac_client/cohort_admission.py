"""Execute the real generated cohort + admission, including a hardware-only build."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix='cse-exact-cohort-') as directory:
    stage = Path(directory)
    for name, contents in [('hw', b'hardware image'), ('swds', b'software image'),
                           ('vgui', b'font owner'), ('mixed', b'foreign owner')]:
        (stage / (name + '.dylib')).write_bytes(contents)
    for software in (True, False):
        command = ['cmake', '-DCSE_ENGINE=' + str(stage / 'hw.dylib'),
                   '-DCSE_VGUI=' + str(stage / 'vgui.dylib'),
                   '-DCSE_HEADER=' + str(stage / 'cse_native_cohort_images.h')]
        if software:
            command += ['-DCSE_SOFTWARE_ENGINE=' + str(stage / 'swds.dylib')]
        subprocess.run(command + ['-P', str(root / 'cmake/WriteNativeCohort.cmake')], check=True)
        subprocess.run(['c++', '-std=c++17', '-DCSE_NATIVE_RUNTIME_BOOTSTRAP=1',
                        '-Wno-deprecated-declarations', '-I' + str(stage),
                        '-I' + str(root / 'GoldSrc-SDK/launcher/native'),
                        str(root / 'tests/native/mac_client/cohort_admission.cpp'),
                        str(root / 'GoldSrc-SDK/launcher/native/MacRuntimeSession.cpp'),
                        str(root / 'GoldSrc-SDK/launcher/native/VGui1HostImageOwner.cpp'),
                    str(root / 'GoldSrc-SDK/launcher/native/SocketImageOwner.cpp'),
                        '-o', str(stage / 'admit')], check=True)
        subprocess.run([str(stage / 'admit'), str(stage), 'software' if software else 'hardware'], check=True)
        foreign = stage / 'foreign'
        foreign.mkdir(exist_ok=True)
        source = foreign / 'image.cpp'
        source.write_text('extern "C" int ForeignImage() { return 1; }\n')
        for name in ('hw.dylib', 'swds.dylib', 'vgui.dylib'):
            subprocess.run(['c++', '-dynamiclib', str(source), '-o', str(foreign / name)], check=True)
            subprocess.run([str(stage / 'admit'), str(stage), 'software' if software else 'hardware',
                            str(foreign / name)], check=True)
            # dyld can reuse a renamed image by LC_ID_DYLIB before constructors,
            # even though both owned files on disk match their exact hashes.
            renamed = foreign / 'renamed.dylib'
            subprocess.run(['c++', '-dynamiclib', str(source), '-install_name', '@rpath/' + name,
                            '-o', str(renamed)], check=True)
            for alias in (renamed, foreign / 'hardlink.dylib', foreign / 'symlink.dylib'):
                if alias != renamed:
                    alias.unlink(missing_ok=True)
                    if alias.name == 'hardlink.dylib':
                        alias.hardlink_to(renamed)
                    else:
                        alias.symlink_to(renamed)
                subprocess.run([str(stage / 'admit'), str(stage), 'software' if software else 'hardware',
                                str(alias)], check=True)


print('PASS exact distinct hardware/software/VGUI admission and software exclusion')
