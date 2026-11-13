"""Explicit pinned-provider staging check. Builds a policy fixture; never executes the provider."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def run(arguments, expected=0):
    result = subprocess.run(arguments, capture_output=True, text=True)
    if expected == 0 and result.returncode != 0:
        raise RuntimeError(result.stdout + result.stderr)
    return result


def check(provider, target):
    target_os, architecture = target.split('-')
    lock = json.loads((ROOT / 'tools/qualification/fixtures/steam/lock.json').read_text())
    profile = lock['windows_x64' if target == 'windows-x64' else 'macos_arm64']['runtime']
    expected = profile['sha256']
    if hashlib.sha256(provider.read_bytes()).hexdigest() != expected:
        raise ValueError('Explicit provider does not match the selected pin')
    with tempfile.TemporaryDirectory(prefix='cse-steam-staging-') as directory:
        root = Path(directory)
        source = root / 'source'
        build = root / 'build'
        source.mkdir()
        copied = root / 'owned-provider'
        shutil.copy2(provider, copied)
        (source / 'probe.cpp').write_text('''#ifndef CSE_OWNED_STEAM_PROVIDER
#error Native architecture policy must select owned Steam routing
#endif
int main() {
    return 0;
}
''')
        (source / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.25)
include("${CSE_SOURCE_ROOT}/cmake/PlatformDefaults.cmake")
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CSE_SOURCE_ROOT}/cmake/MSVCDefaults.cmake")
project(CseSteamStagingPolicy LANGUAGES CXX)
include("${CSE_SOURCE_ROOT}/cmake/TargetPlatform.cmake")
set(CSE_SDK_DIR "${CSE_SOURCE_ROOT}/src")
set(CSE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime")
include("${CSE_SOURCE_ROOT}/cmake/RuntimeTargets.cmake")
add_executable(native_policy_probe probe.cpp "${CSE_SDK_DIR}/common/steam_service.cpp")
cse_link_steam(native_policy_probe Release "${CSE_SDK_DIR}/lib/public/steam_api.lib")
''')
        run(['cmake', '-S', str(source), '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
             f'-DCSE_SOURCE_ROOT={ROOT.as_posix()}', f'-DCSE_TARGET_ARCH={architecture}',
             f'-DCSE_STEAM_PROVIDER_FILE={copied.as_posix()}'])
        run(['cmake', '--build', str(build), '--parallel', '2'])
        staged = build / 'runtime' / Path(profile['path']).name
        assert staged.is_file() and hashlib.sha256(staged.read_bytes()).hexdigest() == expected
        assert list(staged.parent.iterdir()) == [staged], 'Only the runtime provider belongs in staged output'
        staged.unlink()
        run(['cmake', '--build', str(build), '--parallel', '2'])
        assert staged.is_file() and hashlib.sha256(staged.read_bytes()).hexdigest() == expected
        staged.unlink()
        before = copied.stat()
        altered = bytearray(copied.read_bytes())
        altered[-1] ^= 1
        copied.write_bytes(altered)
        # Hide the content change from timestamp-only reconfiguration so the fresh build-time gate must catch it.
        os.utime(copied, ns=(before.st_atime_ns, before.st_mtime_ns))
        rejected = run(['cmake', '--build', str(build), '--parallel', '2'], expected=1)
        assert rejected.returncode != 0
        assert 'Pinned Steam provider hash/size mismatch' in rejected.stdout + rejected.stderr
        assert not staged.exists(), 'Changed input must fail before runtime staging'
    print('Native routing flag, exact payload staging, deleted-file restoration and same-timestamp tamper rejection pass')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--provider', required=True, type=Path)
    parser.add_argument('--target', required=True, choices=['windows-x64', 'macos-arm64'])
    arguments = parser.parse_args()
    check(arguments.provider, arguments.target)
