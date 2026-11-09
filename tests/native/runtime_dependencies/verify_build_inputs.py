"""Check compiler-observed headers and actual native archive link commands."""
from pathlib import Path
import subprocess
import sys

def normalize(text):
    text = text.replace('\\', '/')
    return text.lower() if sys.platform == 'win32' else text


def ninja_dependency_paths(dependencies, build):
    """Resolve indented Ninja dependency records, which may be build-relative."""
    build = Path(build).resolve()
    paths = set()
    for line in dependencies.splitlines():
        if not line.startswith(('    ', '\t')) or not line.strip():
            continue
        path = Path(line.strip().replace('\\', '/'))
        if not path.is_absolute():
            path = build / path
        paths.add(path.resolve())
    return paths


def main(arguments):
    build = Path(arguments[0]).resolve()
    ninja = arguments[1]
    prefix = Path(arguments[2]).resolve()
    configuration = arguments[3]
    architecture = arguments[4]
    library_root = prefix / ('debug/lib' if configuration == 'Debug' else 'lib')
    commands = subprocess.check_output([ninja, '-C', str(build), '-t', 'commands'], text=True)
    dependencies = subprocess.check_output([ninja, '-C', str(build), '-t', 'deps'], text=True)
    Path(build, 'observed-native-link-commands.txt').write_text(commands, encoding='utf-8')
    Path(build, 'observed-native-header-dependencies.txt').write_text(dependencies, encoding='utf-8')
    normalized_commands = normalize(commands)
    dependency_paths = ninja_dependency_paths(dependencies, build)
    normalized_dependencies = {normalize(str(path)) for path in dependency_paths}
    Path(build, 'observed-native-resolved-header-dependencies.txt').write_text(
        '\n'.join(sorted(normalized_dependencies)) + '\n', encoding='utf-8')
    selections = (build / ('selected-imports-' + configuration + '.txt')).read_text().splitlines()
    if len(selections) != 12:
        raise SystemExit('Expected twelve separately selected native package archives')
    identities = []
    for selection in selections:
        family, filename = selection.split('|', 1)
        path = Path(filename).resolve()
        if not path.is_file() or not path.is_relative_to(library_root):
            raise SystemExit(f'{family}: wrong native import configuration or prefix: {path}')
        if normalize(str(path)) not in normalized_commands:
            raise SystemExit(f'{family}: selected native archive absent from actual link commands: {path}')
        suffix = '.exe' if sys.platform == 'win32' else ''
        executable = build / ('runtime_' + family + suffix)
        identity_tool = Path(__file__).resolve().parents[3] / 'tools/binary_identity.py'
        identity = subprocess.check_output(
            [sys.executable, str(identity_tool), '--expect', architecture, str(executable)], text=True)
        identities.append(family + '|' + identity.strip())
    Path(build, 'observed-native-binary-identities.txt').write_text('\n'.join(identities) + '\n', encoding='utf-8')
    for header in ['freetype/config/ftconfig.h', 'zconf.h', 'webm/mkvparser/mkvparser.h']:
        path = (prefix / 'include' / header).resolve()
        if not path.is_file() or not path.is_relative_to(prefix / 'include'):
            raise SystemExit('Native header is missing or escapes the pinned prefix: ' + str(path))
        expected = normalize(str(path))
        if expected not in normalized_dependencies:
            raise SystemExit('Compiler did not use the selected native header: ' + expected)
    if ('x86-windows-static-md' in normalized_commands or 'x86-windows-static-md' in normalize(dependencies)
            or any('x86-windows-static-md' in path for path in normalized_dependencies)):
        raise SystemExit('Retained x86 includes or libraries leaked into native compiler/link inputs')
    print('All twelve native binaries/link inputs and native FreeType/zlib/WebM headers match '
          + architecture + ' ' + configuration)


if __name__ == '__main__':
    main(sys.argv[1:])
