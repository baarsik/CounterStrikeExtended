#!/usr/bin/env python3
"""Static SDL3 source/provider closure audit and explicit isolated game-data fixture.

PE parsing uses import/delay-import directories, never executable string guesses.
Repository ELF/Mach-O/COFF archive inspection additionally requires llvm-readobj.
This does not execute providers or establish dynamic LoadLibrary/runtime acceptance.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
SDL3_DLL_SHA256 = '6d7e653aa9c762ceda4fffc054a504dcce273e016eb9f1fe079b6409a058a9f7'
HISTORY_SHA256 = 'fdf26d0c6987a707d18470ef699e020647acf1c047f527a3427eafaa0cdb7ac2'
REQUIRED_RUNTIME = ['hl.exe', 'hw.dll', 'filesystem_stdio.dll', 'vgui.dll', 'vgui2.dll', 'tier0.dll',
                    'Core.dll', 'DemoPlayer.dll', 'hlds.exe', 'swds.dll', 'valve/cl_dlls/GameUI.dll',
                    'valve/cl_dlls/particleman.dll', 'valve/cl_dlls/client.dll', 'valve/dlls/hl.dll',
                    'cstrike/cl_dlls/client.dll', 'cstrike/dlls/mp.dll']
# OS-owned imports only. Unknown third-party imports fail rather than disappearing into an allow-all rule.
SYSTEM_DLLS = set('''advapi32 avrt bcrypt cabinet cfgmgr32 comctl32 comdlg32 crypt32 cryptbase d2d1 d3d9
    d3d11 d3d12 dbghelp dciman32 dcomp devobj dhcpcsvc dinput dinput8 dnsapi dsound dwmapi dxgi dxguid
    dxva2 gdi32 hid imm32 iphlpapi kernel32 kernelbase mf mfplat mfreadwrite mfuuid msacm32 msvcp_win
    msvcrt msimg32 netapi32 ntdll ole32 oleacc oleaut32 opengl32 powrprof propsys psapi rasapi32 rpcrt4
    secur32 setupapi shell32 shlwapi user32 userenv usp10 ucrtbase uxtheme version win32u windowscodecs
    winhttp wininet winmm winspool wintrust wldap32 ws2_32 wsock32 wtsapi32 xinput1_4 xinput9_1_0
    normaliz ncrypt cryptsp msasn1 winmmbase glu32 oledlg mpr urlmon winusb'''.split())


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def pe_info(data):
    """Read PE32/PE32+ architecture and both formal DLL import directory kinds."""
    def unpack(fmt, offset):
        if offset < 0 or offset + struct.calcsize(fmt) > len(data):
            raise ValueError('Truncated PE record')
        return struct.unpack_from(fmt, data, offset)

    if data[:2] != b'MZ':
        raise ValueError('Not a PE image')
    pe_offset, = unpack('<I', 60)
    if data[pe_offset:pe_offset + 4] != b'PE\0\0':
        raise ValueError('Invalid PE signature')
    machine, section_count = unpack('<HH', pe_offset + 4)
    optional_bytes, = unpack('<H', pe_offset + 20)
    optional = pe_offset + 24
    magic, = unpack('<H', optional)
    if magic == 0x10b:
        image_base, = unpack('<I', optional + 28)
        directory_offset = 96
    elif magic == 0x20b:
        image_base, = unpack('<Q', optional + 24)
        directory_offset = 112
    else:
        raise ValueError('Unsupported PE optional header')
    if optional_bytes < directory_offset or optional + optional_bytes > len(data):
        raise ValueError('Invalid PE optional header bounds')
    directory_count, = unpack('<I', optional + directory_offset - 4)
    if directory_count > (optional_bytes - directory_offset) // 8:
        raise ValueError('PE data directories exceed optional header')
    header_bytes, = unpack('<I', optional + 60)
    sections = []
    for index in range(section_count):
        section = optional + optional_bytes + index * 40
        unpack('<40s', section)
        virtual_bytes, rva, raw_bytes, raw_offset = unpack('<IIII', section + 8)
        if raw_offset + raw_bytes > len(data):
            raise ValueError('PE section exceeds file')
        sections.append((rva, virtual_bytes, raw_bytes, raw_offset))

    def offset_of(rva, size=1):
        if rva < header_bytes and rva + size <= min(header_bytes, len(data)):
            return rva
        for start, virtual_bytes, raw_bytes, raw_offset in sections:
            relative = rva - start
            if 0 <= relative and relative + size <= raw_bytes and relative < max(virtual_bytes, raw_bytes):
                return raw_offset + relative
        raise ValueError(f'Unmapped PE RVA {rva:#x}')

    def name_at(rva):
        name = bytearray()
        for index in range(4096):
            byte = data[offset_of(rva + index)]
            if byte == 0:
                if not name:
                    raise ValueError('Empty PE import name')
                try:
                    return name.decode('ascii')
                except UnicodeDecodeError as error:
                    raise ValueError('Invalid PE import name') from error
            name.append(byte)
        raise ValueError('Unterminated PE import name')

    imports = []
    delay_imports = []
    for index, record_bytes in [(1, 20), (13, 32)]:
        if directory_count <= index:
            continue
        rva, size = unpack('<II', optional + directory_offset + index * 8)
        if not rva and not size:
            continue
        if not rva or size < record_bytes:
            raise ValueError('Invalid PE import directory')
        terminated = False
        for relative in range(0, size - record_bytes + 1, record_bytes):
            fields = unpack('<' + 'I' * (record_bytes // 4), offset_of(rva + relative, record_bytes))
            if not any(fields):
                terminated = True
                break
            name_rva = fields[3] if index == 1 else fields[1]
            if index == 13:
                if fields[0] & ~1:
                    raise ValueError('Unsupported PE delay-import attributes')
                if not fields[0] & 1:
                    name_rva -= image_base
            name = name_at(name_rva)
            imports.append(name)
            if index == 13:
                delay_imports.append(name)
        if not terminated:
            raise ValueError('Unterminated PE import directory')
    forwarders = []
    if directory_count:
        export_rva, export_bytes = unpack('<II', optional + directory_offset)
        if export_rva or export_bytes:
            if not export_rva or export_bytes < 40:
                raise ValueError('Invalid PE export directory')
            export_offset = offset_of(export_rva, 40)
            function_count, = unpack('<I', export_offset + 20)
            functions_rva, = unpack('<I', export_offset + 28)
            if function_count:
                functions_offset = offset_of(functions_rva, function_count * 4)
                for index in range(function_count):
                    function_rva, = unpack('<I', functions_offset + index * 4)
                    if export_rva <= function_rva < export_rva + export_bytes:
                        forwarder = name_at(function_rva)
                        if '.' not in forwarder:
                            raise ValueError('Invalid PE export forwarder')
                        module, symbol = forwarder.rsplit('.', 1)
                        if not module or not symbol:
                            raise ValueError('Invalid PE export forwarder')
                        dependency = module if module.lower().endswith('.dll') else module + '.dll'
                        imports.append(dependency)
                        forwarders.append(forwarder)
    return {'format': 'PE', 'machine': machine, 'imports': sorted(set(imports)),
            'delay_imports': sorted(set(delay_imports)), 'export_forwarders': sorted(set(forwarders))}


def forbidden(name):
    return bool(re.search(r'(?i)(?:^|[/\\])(?:lib)?sdl2(?:d)?(?:[.\-_]|$)|sdl[23][-_]compat', name))


def require_native_sdl3(name, info, enforce_x86=True):
    if forbidden(name) or any(forbidden(edge) for edge in info['imports']):
        raise ValueError(f'SDL2/compatibility dependency: {name}: {info["imports"]}')
    if enforce_x86 and info['format'] == 'PE' and info['machine'] != 0x14c:
        raise ValueError(f'Windows provider is not x86: {name}: {info["machine"]:#x}')


def check_source(name, text):
    # History/prose references are not active dependencies; comments are removed before inspection.
    text = re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S)
    if (re.search(r'#\s*include\s*[<"][^>"\n]*(?:sdl2|SDL_oldnames\.h)', text, re.I)
            or re.search(r'#\s*include\s*[<"]SDL(?:_[^>/"]+)?\.h[>"]', text)
            or re.search(r'\bSDL_ENABLE_OLD_NAMES\b', text)
            or re.search(r'(?i)(?:SDL2\.(?:lib|dll)|external[/\\]+sdl2|\-lSDL2\b)', text)):
        raise ValueError(f'Active SDL2 or compatibility source/build input: {name}')


def tracked_files(root):
    output = subprocess.check_output(['git', '-C', str(root), 'ls-files', '-z'])
    return [root / name.decode() for name in output.split(b'\0') if name and (root / name.decode()).is_file()]


def binary_kind(path):
    with path.open('rb') as source:
        magic = source.read(8)
    if magic[:2] == b'MZ':
        return 'PE'
    if magic == b'!<arch>\n':
        return 'archive'
    if magic[:4] == b'\x7fELF':
        return 'ELF'
    if magic[:4] in [b'\xce\xfa\xed\xfe', b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xce',
                     b'\xfe\xed\xfa\xcf', b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca']:
        return 'Mach-O'
    return None


def inspect_binary(path, llvm_readobj):
    kind = binary_kind(path)
    if kind == 'PE':
        return pe_info(path.read_bytes())
    result = subprocess.run([llvm_readobj, '--file-headers', '--needed-libs', '--coff-imports',
                             str(path)], capture_output=True, text=True)
    if result.returncode:
        raise ValueError(f'Binary inspection failed for {path}: {result.stderr}')
    dependencies = re.findall(r'^\s*(?:File:|Name:|DLLName:)\s*(\S+\.dll)\s*$', result.stdout, re.M | re.I)
    for block in re.findall(r'NeededLibraries \[(.*?)\]', result.stdout, re.S):
        dependencies.extend(line.strip() for line in block.splitlines() if line.strip())
    machines = re.findall(r'Machine: IMAGE_FILE_MACHINE_(\w+)', result.stdout)
    formats = re.findall(r'^Format: (COFF[^\n]+)', result.stdout, re.M)
    return {'format': kind, 'imports': sorted(set(dependencies)), 'coff_machines': sorted(set(machines)), 'coff_formats': sorted(set(formats))}


def audit_repository(root, llvm_readobj):
    files = tracked_files(root)
    records = {}
    active_libraries = set()
    for target in (root / 'cmake/targets').rglob('CMakeLists.txt'):
        for name in re.findall(r'cse_link_import\([^\n]*?"\$\{CSE_SDK_DIR\}/([^"\n]+)"', target.read_text()):
            active_libraries.add('GoldSrc-SDK/' + name)
    active_libraries.update({'GoldSrc-SDK/external/SDL3/lib/x86/SDL3.lib',
                             'GoldSrc-SDK/external/SDL3/lib/x86/SDL3.dll'})
    checked = 0
    source_suffixes = {'.h', '.hpp', '.c', '.cpp', '.cc', '.cxx', '.props', '.targets', '.vcxproj', '.cmake', '.inc', '.inl', '.mk'}
    for path in files:
        relative = path.relative_to(root).as_posix()
        if relative.startswith('GoldSrc-SDK/external/SDL3/'):
            # Upstream headers intentionally contain migration notes and the unmodified oldnames header.
            if path.suffix.lower() not in {'.dll', '.lib'}:
                continue
        if forbidden(relative):
            raise ValueError(f'Retained SDL2/compatibility artifact: {relative}')
        if (path.suffix.lower() in source_suffixes or path.name.startswith(('CMakeLists', 'Makefile'))
                or relative in {'docs/msbuild/repository-selections.json', 'tools/translate_msbuild.py'}):
            check_source(relative, path.read_text(errors='replace'))
            checked += 1
        kind = binary_kind(path)
        if kind:
            info = inspect_binary(path, llvm_readobj)
            active = relative in active_libraries
            require_native_sdl3(relative, info, enforce_x86=active)
            if active and info['format'] == 'archive':
                if not info['coff_formats'] or any(not value.endswith('i386') for value in info['coff_formats']):
                    raise ValueError(f'Active archive is not x86 COFF: {relative}: {info}')
            info['scope'] = 'active Windows provider' if active else 'retained artifact/tooling'
            records[relative] = info | {'sha256': sha256(path)}
    if not active_libraries <= set(records):
        raise ValueError(f'Active library/provider missing from binary audit: {active_libraries - set(records)}')
    sdl_import = records['GoldSrc-SDK/external/SDL3/lib/x86/SDL3.lib']
    if sdl_import['imports'] != ['SDL3.dll']:
        raise ValueError('Pinned SDL3 import archive does not identify SDL3.dll')
    historical = root / 'docs/msbuild/upstream-manifests.zip'
    if sha256(historical) != HISTORY_SHA256:
        raise ValueError('Immutable upstream manifest archive changed')
    return {'source_files_checked': checked, 'binaries': records, 'historical_manifest_sha256': sha256(historical)}


def is_system_dll(name):
    lower = name.lower()
    return lower.startswith(('api-ms-win-', 'ext-ms-win-')) or lower.removesuffix('.dll') in SYSTEM_DLLS


def audit_runtime(root):
    records = {}
    available = set()
    locations = {}
    for path in sorted(root.rglob('*')):
        if not path.is_file():
            continue
        relative = path.relative_to(root).as_posix()
        if forbidden(relative):
            raise ValueError(f'SDL2/compatibility artifact in runtime: {relative}')
        kind = binary_kind(path)
        if not kind:
            continue
        if kind != 'PE':
            raise ValueError(f'Non-PE native artifact in Windows runtime: {relative}')
        info = pe_info(path.read_bytes())
        require_native_sdl3(relative, info)
        records[relative] = info | {'sha256': sha256(path)}
        available.add(path.name.lower())
        locations[relative.lower()] = relative
    if not records or 'sdl3.dll' not in available:
        raise ValueError('Runtime has no native SDL3.dll provider')
    for relative, info in records.items():
        if Path(relative).name.lower() == 'sdl3.dll' and info['sha256'] != SDL3_DLL_SHA256:
            raise ValueError(f'Runtime provider differs from pinned SDL3 3.4.20 x86 DLL: {relative}')
        for edge in info['imports']:
            sibling = (Path(relative).parent / edge).as_posix().lower()
            if edge.lower() not in locations and sibling not in locations and not is_system_dll(edge):
                raise ValueError(f'Unresolved non-system import in application/consumer directory: {relative} -> {edge}')
    return {'binaries': records, 'system_imports': sorted({edge for info in records.values()
            for edge in info['imports'] if is_system_dll(edge)}),
            'limits': 'Formal import/delay-import/export-forwarder closure only; runtime-loaded providers require live module evidence.'}


def safe_relative(name):
    path = Path(name)
    if path.is_absolute() or '..' in path.parts or '\\' in name or not name:
        raise ValueError(f'Unsafe relative fixture path: {name}')
    return name


def overlay_inventory(data):
    if not data.is_dir():
        raise ValueError(f'Game data directory does not exist: {data}')
    return {path.relative_to(data).as_posix(): {'sha256': sha256(path), 'format': binary_kind(path)}
            for path in data.rglob('*') if path.is_file() and binary_kind(path)}


def prepare_fixture(data, runtime, destination, decisions):
    """Copy all non-native game data; native overlays require exhaustive explicit decisions."""
    data = data.resolve()
    runtime = runtime.resolve()
    destination = destination.resolve()
    if not data.is_dir() or not runtime.is_dir():
        raise ValueError('Fixture data/runtime source directory does not exist')
    if not isinstance(decisions, dict) or set(decisions) - {'providers', 'excluded'}:
        raise ValueError('Invalid fixture decisions object')
    if not isinstance(decisions.get('providers', []), list) or not isinstance(decisions.get('excluded', {}), dict):
        raise ValueError('Fixture providers must be a list and exclusions an object')
    for source in (data, runtime):
        if source == destination or source in destination.parents or destination in source.parents:
            raise ValueError('Fixture must be separate from data/runtime sources')
    if destination.exists() and any(destination.iterdir()):
        raise ValueError('Fixture destination must be empty')
    hashes = {}
    for relative in REQUIRED_RUNTIME + ['SDL3.dll']:
        path = runtime / relative
        if not path.is_file():
            raise ValueError(f'Missing source-built runtime output: {relative}')
        require_native_sdl3(relative, pe_info(path.read_bytes()))
        hashes[relative] = sha256(path)
    inventory = overlay_inventory(data)
    providers = {safe_relative(name) for name in decisions.get('providers', [])}
    excluded = {safe_relative(name): reason for name, reason in decisions.get('excluded', {}).items()}
    replacements = {name for name in inventory if (runtime / name).is_file()}
    if providers & set(excluded) or providers & replacements or set(excluded) & replacements:
        raise ValueError('Conflicting native overlay decisions')
    if providers | set(excluded) | replacements != set(inventory):
        raise ValueError('Every native data-overlay artifact requires a provider/exclusion/replacement decision')
    if any(not isinstance(reason, str) or not reason.strip() for reason in excluded.values()):
        raise ValueError('Each excluded artifact requires a documented reason')
    for name in providers:
        require_native_sdl3(name, pe_info((data / name).read_bytes()))
    destination.mkdir(parents=True, exist_ok=True)
    for path in data.rglob('*'):
        if not path.is_file():
            continue
        name = path.relative_to(data).as_posix()
        if name in excluded or name in replacements:
            continue
        target = destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
    for path in runtime.rglob('*'):
        if path.is_file():
            target = destination / path.relative_to(runtime)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
    report = audit_runtime(destination)
    for name, expected in hashes.items():
        if sha256(destination / name) != expected:
            raise ValueError(f'Source-built runtime hash mismatch: {name}')
    return report | {'runtime_hashes': hashes, 'providers': {name: inventory[name] for name in sorted(providers)},
                     'excluded': {name: inventory[name] | {'reason': excluded[name]} for name in sorted(excluded)},
                     'replaced_by_runtime': sorted(replacements)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    repository = sub.add_parser('repository')
    repository.add_argument('--root', type=Path, default=ROOT)
    repository.add_argument('--llvm-readobj', default='llvm-readobj')
    runtime = sub.add_parser('runtime')
    runtime.add_argument('directory', type=Path)
    inventory = sub.add_parser('inventory')
    inventory.add_argument('data', type=Path)
    fixture = sub.add_parser('fixture')
    fixture.add_argument('--data', type=Path, required=True)
    fixture.add_argument('--runtime', type=Path, required=True)
    fixture.add_argument('--destination', type=Path, required=True)
    fixture.add_argument('--decisions', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == 'repository':
            report = audit_repository(args.root, args.llvm_readobj)
        elif args.command == 'runtime':
            report = audit_runtime(args.directory)
        elif args.command == 'inventory':
            report = overlay_inventory(args.data)
        else:
            report = prepare_fixture(args.data, args.runtime, args.destination, json.loads(args.decisions.read_text()))
        args.output.write_text(json.dumps(report, indent=2) + '\n')
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(f'SDL3 audit failed: {error}', file=sys.stderr)
        return 1
    print(f'SDL3 {args.command} evidence: {args.output}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
