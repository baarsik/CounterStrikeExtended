#!/usr/bin/env python3
"""Read target architectures from binary contents; never load or execute them.

PE/COFF: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
Mach-O: Apple mach-o/loader.h and mach-o/fat.h. Unknown formats, LLVM bitcode,
ARM64EC and thin archives are deliberately unsupported rather than guessed.
"""
import argparse
import json
from pathlib import Path
import struct
import sys

COFF_CPUS = {0x014c: 'x86', 0x8664: 'x64', 0xaa64: 'arm64'}
MACH_CPUS = {7: 'x86', 0x01000007: 'x64', 0x0100000c: 'arm64'}
ELF_CPUS = {3: 'x86', 62: 'x64', 183: 'arm64'}


def _unpack(data, fmt, offset=0):
    if offset < 0 or offset + struct.calcsize(fmt) > len(data):
        raise ValueError('Truncated binary header')
    return struct.unpack_from(fmt, data, offset)


def _cpu(table, machine):
    try:
        return table[machine]
    except KeyError as error:
        raise ValueError(f'Unsupported machine {machine:#x}') from error


def _width(architecture, wide):
    if (architecture != 'x86') != wide:
        raise ValueError(f'{architecture}: header pointer width mismatch')


def _result(kind, architectures, **extra):
    return dict(format=kind, architectures=sorted(set(architectures)), **extra)


def _extent(data, offset, size, label):
    if offset < 0 or size < 0 or offset + size > len(data):
        raise ValueError('Truncated binary ' + label + ' extent/body')


def _coff_sections(data, start, count, *, pe=False):
    _extent(data, start, count * 40, 'section table')
    for index in range(count):
        section = start + index * 40
        size, offset, relocations, lines = _unpack(data, '<IIII', section + 16)
        relocation_count, line_count, flags = _unpack(data, '<HHI', section + 32)
        # Uninitialized COFF data occupies memory, not file bytes.
        if size and not (not pe and offset == 0 and flags & 0x80):
            _extent(data, offset, size, 'section')
        if not pe:
            if relocation_count:
                _extent(data, relocations, relocation_count * 10, 'COFF relocations')
            if line_count:
                _extent(data, lines, line_count * 6, 'COFF line records')


def _coff_symbols(data, offset, count, entry_size):
    if count:
        _extent(data, offset, count * entry_size, 'COFF symbols')
        strings = offset + count * entry_size
        size, = _unpack(data, '<I', strings)
        # MSVC import-descriptor members (including pinned FreeType) encode
        # an empty string table as a zero length plus its four-byte header.
        if size == 0:
            size = 4
        if size < 4:
            raise ValueError('Invalid COFF string table extent')
        _extent(data, strings, size, 'COFF strings')


def looks_native(data):
    """Recognize image/archive magic, for extension-independent inventory scans."""
    return (data.startswith((b'MZ', b'!<arch>\n', b'!<thin>\n', b'\x7fELF'))
            or bytes(data[:4]) in (b'\xce\xfa\xed\xfe', b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xce',
                                  b'\xfe\xed\xfa\xcf', b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca',
                                  b'\xca\xfe\xba\xbf', b'\xbf\xba\xfe\xca'))


def _archive(data):
    offset = 8
    architectures = []
    objects = 0
    minimums = []
    while offset < len(data):
        header, = _unpack(data, '60s', offset)
        if header[58:] != b'`\n':
            raise ValueError('Invalid archive member header')
        try:
            name = header[:16].decode('ascii').rstrip()
            size_text = header[48:58].decode('ascii').strip()
            if not size_text.isdecimal():
                raise ValueError('Invalid archive member size')
            size = int(size_text)
        except UnicodeDecodeError as error:
            raise ValueError('Invalid archive member header') from error
        offset += 60
        if offset + size > len(data):
            raise ValueError('Truncated archive member')
        payload = data[offset:offset + size]
        # BSD extended names are part of the member bytes, not the object.
        if name.startswith('#1/'):
            try:
                name_size = int(name[3:])
            except ValueError as error:
                raise ValueError('Invalid archive extended name') from error
            if name_size < 0 or name_size > size:
                raise ValueError('Invalid archive extended name')
            name = payload[:name_size].rstrip(b'\0').decode('ascii')
            payload = payload[name_size:]
        if name not in ('/', '//', '/SYM64/', '__.SYMDEF', '__.SYMDEF SORTED'):
            member = inspect_bytes(payload)
            if member['format'] not in ('coff', 'coff-import', 'coff-bigobj', 'mach-o', 'elf'):
                raise ValueError(f'Unsupported archive member: {name}')
            architectures.extend(member['architectures'])
            minimums.append(member.get('minimum_macos'))
            objects += 1
        offset += size
        if offset % 2:
            if data[offset:offset + 1] != b'\n':
                raise ValueError('Missing archive member padding')
            offset += 1
    if not objects:
        raise ValueError('Archive contains no inspectable objects')
    metadata = dict(object_count=objects)
    if all(minimum is not None for minimum in minimums):
        metadata['minimum_macos'] = list(max(tuple(minimum) for minimum in minimums))
    return _result('archive', architectures, **metadata)


def _macho(data, endian, wide):
    header_size = 32 if wide else 28
    if len(data) < header_size:
        raise ValueError('Truncated Mach-O header')
    machine, _, _, commands, command_bytes = _unpack(data, endian + 'IIIII', 4)
    _extent(data, header_size, command_bytes, 'Mach-O load commands')
    offset = header_size
    command_end = header_size + command_bytes
    minimum_macos = None
    for _ in range(commands):
        command, size = _unpack(data, endian + 'II', offset)
        if size < 8 or offset + size > command_end:
            raise ValueError('Invalid Mach-O load command extent')
        if command in (0x1, 0x19):
            segment_wide = command == 0x19
            fixed_size = 72 if segment_wide else 56
            section_size = 80 if segment_wide else 68
            if size < fixed_size:
                raise ValueError('Invalid Mach-O segment command extent')
            file_offset, file_size = _unpack(data, endian + ('QQ' if segment_wide else 'II'),
                                             offset + (40 if segment_wide else 32))
            _extent(data, file_offset, file_size, 'Mach-O segment')
            sections, = _unpack(data, endian + 'I', offset + (64 if segment_wide else 48))
            if fixed_size + sections * section_size > size:
                raise ValueError('Invalid Mach-O section table extent')
            for index in range(sections):
                section = offset + fixed_size + index * section_size
                section_bytes, = _unpack(data, endian + ('Q' if segment_wide else 'I'),
                                         section + (40 if segment_wide else 36))
                section_offset, _, relocations, relocation_count, flags = _unpack(
                    data, endian + 'IIIII', section + (48 if segment_wide else 40))
                if (flags & 0xff) not in (0x1, 0xc, 0x12):
                    _extent(data, section_offset, section_bytes, 'Mach-O section')
                if relocation_count:
                    _extent(data, relocations, relocation_count * 8, 'Mach-O relocations')
        elif command == 0x2:
            if size < 24:
                raise ValueError('Invalid Mach-O symbol command extent')
            symbols, count, strings, string_bytes = _unpack(data, endian + 'IIII', offset + 8)
            _extent(data, symbols, count * (16 if wide else 12), 'Mach-O symbols')
            _extent(data, strings, string_bytes, 'Mach-O strings')
        elif command in (0x24, 0x32):  # LC_VERSION_MIN_MACOSX / LC_BUILD_VERSION.
            if size < (16 if command == 0x24 else 24):
                raise ValueError('Invalid Mach-O minimum-version command extent')
            if command == 0x32:
                platform, version, _, tools = _unpack(data, endian + 'IIII', offset + 8)
                if 24 + tools * 8 > size:
                    raise ValueError('Invalid Mach-O build-tool command extent')
            else:
                platform = 1 # PLATFORM_MACOS.
                version, = _unpack(data, endian + 'I', offset + 8)
            if platform == 1:
                if minimum_macos is not None:
                    raise ValueError('Duplicate Mach-O macOS minimum-version command')
                minimum_macos = [version >> 16, (version >> 8) & 0xff, version & 0xff]
        offset += size
    if offset != command_end:
        raise ValueError('Mach-O load-command count/extent mismatch')
    architecture = _cpu(MACH_CPUS, machine)
    _width(architecture, wide)
    metadata = dict(minimum_macos=minimum_macos) if minimum_macos is not None else {}
    return _result('mach-o', [architecture], **metadata)


def _fat(data, endian, wide):
    count, = _unpack(data, endian + 'I', 4)
    record_size = 32 if wide else 20
    table_end = 8 + count * record_size
    if not count or table_end > len(data):
        raise ValueError('Invalid universal Mach-O table')
    architectures = []
    extents = []
    minimums = []
    for index in range(count):
        fields = _unpack(data, endian + ('IIQQII' if wide else 'IIIII'), 8 + index * record_size)
        machine, _, offset, size, alignment = fields[:5]
        if not size or offset < table_end or offset + size > len(data) or alignment > 63:
            raise ValueError('Invalid universal Mach-O slice bounds')
        if offset % (1 << alignment) or any(offset < end and offset + size > start for start, end in extents):
            raise ValueError('Invalid universal Mach-O slice alignment/overlap')
        extents.append((offset, offset + size))
        architecture = _cpu(MACH_CPUS, machine)
        member = inspect_bytes(data[offset:offset + size])
        if member['format'] not in ('mach-o', 'archive') or member['architectures'] != [architecture]:
            raise ValueError('Universal Mach-O slice architecture mismatch')
        if architecture in architectures:
            raise ValueError('Duplicate universal Mach-O architecture slice')
        architectures.append(architecture)
        minimums.append(member.get('minimum_macos'))
    metadata = {}
    if all(minimum is not None for minimum in minimums):
        metadata['minimum_macos'] = list(max(tuple(minimum) for minimum in minimums))
    return _result('mach-o-universal', architectures, **metadata)


def inspect_bytes(data):
    """Return format/architectures, or raise ValueError for unknown/invalid input."""
    if data.startswith(b'!<arch>\n'):
        return _archive(data)
    if data.startswith(b'MZ'):
        offset, = _unpack(data, '<I', 60)
        signature, = _unpack(data, '4s', offset)
        if offset < 64 or signature != b'PE\0\0':
            raise ValueError('Invalid PE signature')
        machine, sections, _, symbols, symbol_count, optional_size, _ = _unpack(data, '<HHIIIHH', offset + 4)
        optional = offset + 24
        magic, = _unpack(data, '<H', optional)
        if magic not in (0x10b, 0x20b):
            raise ValueError('Invalid PE optional header')
        minimum = 112 if magic == 0x20b else 96
        if optional_size < minimum or optional + optional_size + sections * 40 > len(data):
            raise ValueError('Truncated PE optional/section headers')
        _coff_sections(data, optional + optional_size, sections, pe=True)
        _coff_symbols(data, symbols, symbol_count, 18)
        architecture = _cpu(COFF_CPUS, machine)
        _width(architecture, magic == 0x20b)
        return _result('pe', [architecture])
    magic = bytes(data[:4])
    mach = {b'\xce\xfa\xed\xfe': ('<', False), b'\xcf\xfa\xed\xfe': ('<', True),
            b'\xfe\xed\xfa\xce': ('>', False), b'\xfe\xed\xfa\xcf': ('>', True)}
    if magic in mach:
        return _macho(data, *mach[magic])
    fat = {b'\xca\xfe\xba\xbe': ('>', False), b'\xbe\xba\xfe\xca': ('<', False),
           b'\xca\xfe\xba\xbf': ('>', True), b'\xbf\xba\xfe\xca': ('<', True)}
    if magic in fat:
        return _fat(data, *fat[magic])
    if magic == b'\x7fELF':
        header, = _unpack(data, '16s')
        if header[4] not in (1, 2) or header[5] not in (1, 2) or header[6] != 1:
            raise ValueError('Invalid ELF identification')
        wide = header[4] == 2
        if len(data) < (64 if wide else 52):
            raise ValueError('Truncated ELF header')
        machine, = _unpack(data, ('<' if header[5] == 1 else '>') + 'H', 18)
        endian = '<' if header[5] == 1 else '>'
        programs, sections = _unpack(data, endian + ('QQ' if wide else 'II'), 32 if wide else 28)
        program_size, program_count, section_size, section_count = _unpack(
            data, endian + 'HHHH', 54 if wide else 42)
        if program_count:
            if program_size < (56 if wide else 32):
                raise ValueError('Invalid ELF program table extent')
            _extent(data, programs, program_size * program_count, 'ELF program table')
            for index in range(program_count):
                base = programs + index * program_size
                file_offset, = _unpack(data, endian + ('Q' if wide else 'I'), base + (8 if wide else 4))
                file_bytes, = _unpack(data, endian + ('Q' if wide else 'I'), base + (32 if wide else 16))
                _extent(data, file_offset, file_bytes, 'ELF segment')
        if section_count:
            if section_size < (64 if wide else 40):
                raise ValueError('Invalid ELF section table extent')
            _extent(data, sections, section_size * section_count, 'ELF section table')
            for index in range(section_count):
                base = sections + index * section_size
                kind, = _unpack(data, endian + 'I', base + 4)
                file_offset, file_bytes = _unpack(data, endian + ('QQ' if wide else 'II'), base + (24 if wide else 16))
                if kind != 8:  # SHT_NOBITS has memory size but no file body.
                    _extent(data, file_offset, file_bytes, 'ELF section')
        architecture = _cpu(ELF_CPUS, machine)
        _width(architecture, wide)
        return _result('elf', [architecture])
    if magic == b'\0\0\xff\xff':
        version, machine, _, size = _unpack(data, '<HHII', 4)
        architecture = _cpu(COFF_CPUS, machine)
        if version == 0:
            _unpack(data, '<20s')
            if size < 2 or len(data) != 20 + size or data[-1:] != b'\0':
                raise ValueError('Invalid COFF short import size')
            return _result('coff-import', [architecture])
        if version == 2:
            # ANON_OBJECT_HEADER_BIGOBJ ClassID, from Microsoft's bigobj format.
            class_id, = _unpack(data, '16s', 12)
            if class_id != bytes.fromhex('c7a1bad1eebaa94baf20faf66aa4dcb8'):
                raise ValueError('Invalid COFF bigobj class ID')
            sections, = _unpack(data, '<I', 44)
            _coff_sections(data, 56, sections)
            symbols, symbol_count = _unpack(data, '<II', 48)
            _coff_symbols(data, symbols, symbol_count, 20)
            return _result('coff-bigobj', [architecture])
        raise ValueError('Unsupported anonymous COFF version')
    if len(data) >= 20:
        machine, sections, _, symbols, symbol_count, optional_size, _ = _unpack(data, '<HHIIIHH')
        if machine in COFF_CPUS:
            if optional_size or not sections or 20 + sections * 40 > len(data):
                raise ValueError('Invalid COFF object section table')
            _coff_sections(data, 20, sections)
            _coff_symbols(data, symbols, symbol_count, 18)
            return _result('coff', [_cpu(COFF_CPUS, machine)])
    raise ValueError('Unsupported binary format/machine')


def require_architecture(data, architecture, *, allow_universal=False, maximum_macos=None):
    if architecture not in ('x86', 'x64', 'arm64'):
        raise ValueError('Unsupported target architecture: ' + architecture)
    result = inspect_bytes(data)
    if maximum_macos is not None:
        minimum = result.get('minimum_macos')
        if minimum is None or tuple(minimum) > tuple(maximum_macos):
            raise ValueError(f'Binary macOS minimum {minimum}; required at most {maximum_macos} for every object')
    actual = result['architectures']
    if actual == [architecture]:
        return result
    if allow_universal and result['format'] == 'mach-o-universal' and architecture in actual:
        return result
    if len(actual) > 1:
        raise ValueError(f'Mixed binary architectures {actual}; expected {architecture}')
    raise ValueError(f'Binary architecture {actual}; expected {architecture}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--expect', choices=['x86', 'x64', 'arm64'])
    parser.add_argument('--allow-universal', action='store_true')
    parser.add_argument('--max-macos', help='Maximum permitted macOS minimum version, e.g. 14.0')
    parser.add_argument('files', nargs='+', type=Path)
    args = parser.parse_args()
    try:
        reports = {}
        maximum = None
        if args.max_macos:
            parts = args.max_macos.split('.')
            if not 1 <= len(parts) <= 3 or not all(part.isdecimal() for part in parts):
                raise ValueError('Invalid maximum macOS version')
            maximum = tuple(map(int, parts)) + (0,) * (3 - len(parts))
            if not args.expect:
                raise ValueError('--max-macos requires --expect')
        for path in args.files:
            data = path.read_bytes()
            reports[str(path)] = (require_architecture(data, args.expect, allow_universal=args.allow_universal,
                                                      maximum_macos=maximum)
                                  if args.expect else inspect_bytes(data))
        print(json.dumps(reports, indent=2))
    except (ValueError, OSError) as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
