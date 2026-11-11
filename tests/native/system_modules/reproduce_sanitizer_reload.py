"""Unrelated macOS AppleClang ASan reload control, without any project source.

Usage: python3 reproduce_sanitizer_reload.py <empty-output-directory>
The current AppleClang21 runtime crashes during globals registration on reload.
"""
from pathlib import Path
import subprocess, sys
out = Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
provider = '\n'.join(f'char value_{i}[32] = "value{i}";' for i in range(1500))
(out/'provider.cpp').write_text(provider)
(out/'reload.cpp').write_text('''#include <dlfcn.h>
#include <cstdio>
int main(int argc, char** argv) {
    for (int index = 0; index < 5; ++index) {
        std::fprintf(stderr, "load cycle %d\\n", index);
        void* image = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        if (!image) { std::puts(dlerror()); return 1; }
        dlclose(image);
    }
}
''')
flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
subprocess.run(['c++', *flags, '-dynamiclib', str(out/'provider.cpp'), '-o', str(out/'provider.dylib')], check=True)
subprocess.run(['c++', *flags, str(out/'reload.cpp'), '-o', str(out/'reload')], check=True)
result = subprocess.run([str(out/'reload'), str(out/'provider.dylib')], capture_output=True, text=True)
(out/'result.log').write_text(result.stdout + result.stderr)
print(f'exit={result.returncode}; log={out / "result.log"}')
sys.exit(0 if result.returncode == 0 else 1)
