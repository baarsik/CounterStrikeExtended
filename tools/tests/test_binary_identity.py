"""Inspect real binary headers and archive members without loading native code."""
import importlib.util
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
try:
    import binary_identity as identity
except ModuleNotFoundError:
    identity = None


def coff(machine):
    return struct.pack('<HHIIIHH', machine, 1, 0, 0, 0, 0, 0) + bytes(40)


def short_import(machine):
    names = b'function\0provider.dll\0'
    return struct.pack('<HHHHIIHH', 0, 0xffff, 0, machine, 0, len(names), 0, 0) + names


def archive(*members):
    data = bytearray(b'!<arch>\n')
    for name, payload in members:
        header = f'{name:<16}{0:<12}{0:<6}{0:<6}{0:<8}{len(payload):<10}`\n'.encode('ascii')
        assert len(header) == 60
        data.extend(header)
        data.extend(payload)
        if len(payload) % 2:
            data.extend(b'\n')
    return bytes(data)


def pe(machine, plus=False):
    data = bytearray(512)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 60, 128)
    data[128:132] = b'PE\0\0'
    optional_size = 240 if plus else 224
    struct.pack_into('<HHIIIHH', data, 132, machine, 0, 0, 0, 0, optional_size, 0)
    struct.pack_into('<H', data, 152, 0x20b if plus else 0x10b)
    return bytes(data)


def macho(cpu, wide=True):
    magic = 0xfeedfacf if wide else 0xfeedface
    return struct.pack('<IIIIIII', magic, cpu, 0, 6, 0, 0, 0) + (bytes(4) if wide else b'')


def macos_object(major, minor=0, patch=0):
    header = struct.pack('<IIIIIIII', 0xfeedfacf, 0x100000c, 0, 1, 1, 24, 0, 0)
    return header + struct.pack('<IIIIII', 0x32, 24, 1, (major << 16) | (minor << 8) | patch, 0, 0)


def elf(machine, wide=True):
    data = bytearray(64 if wide else 52)
    data[:7] = b'\x7fELF' + bytes([2 if wide else 1, 1, 1])
    struct.pack_into('<HHI', data, 16, 3, machine, 1)
    return bytes(data)


class BinaryIdentityTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(identity, 'binary_identity implementation is required')

    def test_pe_architecture_comes_from_header_and_optional_width(self):
        for machine, arch in [(0x14c, 'x86'), (0x8664, 'x64'), (0xaa64, 'arm64')]:
            result = identity.inspect_bytes(pe(machine, plus=arch != 'x86'))
            self.assertEqual(result['format'], 'pe')
            self.assertEqual(result['architectures'], [arch])
        with self.assertRaisesRegex(ValueError, 'width'):
            identity.inspect_bytes(pe(0x8664))

    def test_archive_inspects_every_object_and_import_member(self):
        data = archive(('/', b'linker table'), ('//', b'long object names/\n'),
                       ('one.obj/', coff(0x8664)), ('/0', short_import(0xaa64)))
        result = identity.inspect_bytes(data)
        self.assertEqual(result['format'], 'archive')
        self.assertEqual(result['architectures'], ['arm64', 'x64'])
        self.assertEqual(result['object_count'], 2)
        with self.assertRaisesRegex(ValueError, 'x86'):
            identity.require_architecture(data, 'x86')
        with self.assertRaisesRegex(ValueError, '[Mm]ixed'):
            identity.require_architecture(data, 'x64')

    def test_archive_unknown_empty_truncated_and_thin_fail_closed(self):
        for data in [archive(('/', b'linker table')), archive(('bad.obj/', b'not an object')),
                     archive(('a.obj/', coff(0x14c)))[:-1], b'!<thin>\n']:
            with self.subTest(data=data[:30]), self.assertRaises(ValueError):
                identity.inspect_bytes(data)

    def test_macho_and_elf_widths(self):
        for data, arch, kind in [(macho(0x100000c), 'arm64', 'mach-o'),
                                 (macho(0x1000007), 'x64', 'mach-o'),
                                 (elf(62), 'x64', 'elf'), (elf(183), 'arm64', 'elf'),
                                 (elf(3, False), 'x86', 'elf')]:
            result = identity.require_architecture(data, arch)
            self.assertEqual(result['format'], kind)
        for data in [macho(0x1000007, False), elf(62, False)]:
            with self.assertRaisesRegex(ValueError, 'width'):
                identity.inspect_bytes(data)

    def test_macos_minimum_is_checked_in_every_archive_member(self):
        maximum = (14, 0, 0)
        valid = archive(('one.o/', macos_object(13, 5)), ('two.o/', macos_object(14)))
        result = identity.require_architecture(valid, 'arm64', maximum_macos=maximum)
        self.assertEqual(result['minimum_macos'], [14, 0, 0])
        for incompatible in [macos_object(27), macho(0x100000c), coff(0xaa64)]:
            with self.subTest(incompatible=incompatible[:16]), self.assertRaisesRegex(ValueError, 'macOS'):
                identity.require_architecture(archive(('one.o/', macos_object(14)), ('two.o/', incompatible)),
                                              'arm64', maximum_macos=maximum)

    def test_universal_macho_checks_slice_headers(self):
        x64 = macho(0x1000007)
        arm = macho(0x100000c)
        header = struct.pack('>II', 0xcafebabe, 2)
        header += struct.pack('>IIIII', 0x1000007, 0, 48, len(x64), 0)
        header += struct.pack('>IIIII', 0x100000c, 0, 48 + len(x64), len(arm), 0)
        data = header + x64 + arm
        self.assertEqual(identity.inspect_bytes(data)['architectures'], ['arm64', 'x64'])
        identity.require_architecture(data, 'arm64', allow_universal=True)
        with self.assertRaisesRegex(ValueError, '[Mm]ixed'):
            identity.require_architecture(data, 'arm64')
        bad = bytearray(data)
        struct.pack_into('>I', bad, 28, 0x1000007)
        with self.assertRaisesRegex(ValueError, 'slice'):
            identity.inspect_bytes(bad)

    def test_pinned_msvc_archive_with_empty_string_table_is_accepted(self):
        path = Path(__file__).resolve().parents[2] / 'src/external/installed/x86-windows-static-md/lib/freetype.lib'
        self.assertEqual(identity.require_architecture(path.read_bytes(), 'x86')['architectures'], ['x86'])

    def test_truncated_real_producer_bodies_are_rejected(self):
        provider = Path(__file__).resolve().parents[2] / 'src/external/SDL3/lib/x86/SDL3.dll'
        data = provider.read_bytes()
        pe_offset, = struct.unpack_from('<I', data, 60)
        sections, = struct.unpack_from('<H', data, pe_offset + 6)
        optional, = struct.unpack_from('<H', data, pe_offset + 20)
        header_end = pe_offset + 24 + optional + sections * 40
        with self.assertRaisesRegex(ValueError, 'body|section|extent'):
            identity.inspect_bytes(data[:header_end])
        # A 64-bit segment declares bytes after its load-command region.
        header = struct.pack('<IIIIIIII', 0xfeedfacf, 0x100000c, 0, 6, 1, 72, 0, 0)
        segment = struct.pack('<II16sQQQQIIII', 0x19, 72, b'__TEXT', 0, 256, 0, 256, 5, 5, 0, 0)
        with self.assertRaisesRegex(ValueError, 'body|segment|extent'):
            identity.inspect_bytes(header + segment)
        obj = bytearray(coff(0x8664))
        struct.pack_into('<II', obj, 20 + 16, 16, len(obj))
        with self.assertRaisesRegex(ValueError, 'body|section|extent'):
            identity.inspect_bytes(obj)
        image = bytearray(elf(62))
        struct.pack_into('<Q', image, 40, 64)
        struct.pack_into('<HH', image, 58, 64, 1)
        with self.assertRaisesRegex(ValueError, 'body|section|extent'):
            identity.inspect_bytes(image)

    def test_malformed_headers_and_unknown_machines_are_rejected(self):
        for data in [b'', b'MZ', b'\x7fELF', b'not executable', coff(0x1234), pe(0x1234),
                     macho(0x1234), elf(0x1234), archive(('a.obj/', coff(0x8664))) + b'x']:
            with self.subTest(data=data[:20]), self.assertRaises(ValueError):
                identity.inspect_bytes(data)


if __name__ == '__main__':
    unittest.main()
