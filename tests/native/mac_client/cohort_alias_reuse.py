"""Real dyld dependency reuse plus real preconstructor Session admission."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix='cse-cohort-installname-') as directory:
    stage = Path(directory)
    (stage / 'provider.cpp').write_text('''extern "C" int ProviderMarker() {
    return VALUE;
}
''')
    (stage / 'engine.cpp').write_text('''extern "C" int ProviderMarker();
int value = ProviderMarker();
extern "C" int EngineMarker() {
    return value;
}
''')
    for name, value in [('vgui', 1), ('renamed-provider', 2)]:
        subprocess.run(['c++', '-dynamiclib', '-DVALUE=' + str(value), str(stage / 'provider.cpp'),
                        '-install_name', '@rpath/vgui.dylib', '-o', str(stage / (name + '.dylib'))], check=True)
    subprocess.run(['c++', '-dynamiclib', str(stage / 'engine.cpp'), str(stage / 'vgui.dylib'),
                    '-Wl,-rpath,' + str(stage), '-install_name', '@rpath/hw.dylib',
                    '-o', str(stage / 'hw.dylib')], check=True)
    subprocess.run(['cmake', '-DCSE_ENGINE=' + str(stage / 'hw.dylib'),
                    '-DCSE_VGUI=' + str(stage / 'vgui.dylib'),
                    '-DCSE_HEADER=' + str(stage / 'cse_native_cohort_images.h'),
                    '-P', str(root / 'cmake/WriteNativeCohort.cmake')], check=True)
    subprocess.run(['c++', '-std=c++17', '-DCSE_NATIVE_RUNTIME_BOOTSTRAP=1',
                    '-Wno-deprecated-declarations', '-I' + str(stage),
                    '-I' + str(root / 'GoldSrc-SDK/launcher/native'),
                    str(root / 'tests/native/mac_client/cohort_alias_reuse.cpp'),
                    str(root / 'GoldSrc-SDK/launcher/native/MacRuntimeSession.cpp'),
                    str(root / 'GoldSrc-SDK/launcher/native/VGui1HostImageOwner.cpp'),
                    str(root / 'GoldSrc-SDK/launcher/native/SocketImageOwner.cpp'),
                    '-o', str(stage / 'driver')], check=True)
    subprocess.run([str(stage / 'driver'), str(stage / 'renamed-provider.dylib'),
                    str(stage / 'hw.dylib'), str(stage / 'vgui.dylib')], check=True)
