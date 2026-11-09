"""Run real child CMake configurations and require their specific rejection."""
from pathlib import Path
import subprocess
import sys

source, build, case, prefix, cc, cxx, architecture = sys.argv[1:]
reasons = {
    'unknown-import': 'Unknown Release native runtime logical import',
    'wrong-root': 'Unknown Release native runtime logical import',
    'import-escape': 'Unsupported native runtime reference path',
    'wrong-config': 'Unknown Debug native runtime logical import',
    'unsupported-config': 'Unsupported native runtime import configuration',
    'include-escape': 'Unsupported native runtime reference path',
    'invalid-include': 'Unsupported retained dependency include',
    'missing-include': 'Native runtime include is missing or escapes the pinned prefix',
    'missing-prefix': 'Select an explicit source-built CSE_NATIVE_THIRD_PARTY_PREFIX',
    'foreign-provider': 'resolves outside the native dependency prefix',
    'interface-provider': 'Unknown native runtime dependency provider',
    'configuration-fallback': 'unsupported DEBUG import configuration mapping',
    'cached-provider-change': 'unsupported DEBUG import configuration mapping',
    'sdk-internal-symlink': 'Native runtime include is missing or escapes the pinned prefix',
    'sdk-escape-symlink': 'Native runtime reference path escapes the SDK',
    'wrong-architecture': 'Binary architecture validation failed',
}
command = ['cmake', '-S', source, '-B', build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
           '-DCSE_TARGET_ARCH=' + architecture, '-DCMAKE_C_COMPILER=' + cc, '-DCMAKE_CXX_COMPILER=' + cxx,
           '-DCSE_NATIVE_THIRD_PARTY_PREFIX=' + prefix, '-DCSE_RUNTIME_REJECTION_CASE=' + case]
result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
Path(build).mkdir(parents=True, exist_ok=True)
Path(build, 'expected-rejection.log').write_text(result.stdout, encoding='utf-8')
if result.returncode == 0 or reasons[case] not in result.stdout:
    print(result.stdout)
    raise SystemExit('Missing expected rejection: ' + case + ': ' + reasons[case])
print('Rejected ' + case + ': ' + reasons[case])
