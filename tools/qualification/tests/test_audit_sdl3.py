"""Exercise import records and fail-closed fixture/source policy, without executing PEs."""
import importlib.util
from pathlib import Path
import struct
import shutil
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('audit_sdl3', Path(__file__).parents[1] / 'audit_sdl3.py')
PINNED_DLL = Path(__file__).resolve().parents[3] / 'src/external/SDL3/lib/x86/SDL3.dll'
audit = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(audit)


def pe(import_name='SDL3.dll', delayed=False, machine=0x14c):
    data = bytearray(2048)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 60, 128)
    data[128:132] = b'PE\0\0'
    struct.pack_into('<HH', data, 132, machine, 1)
    struct.pack_into('<H', data, 148, 224)
    struct.pack_into('<H', data, 152, 0x10b)
    struct.pack_into('<I', data, 152 + 28, 0x400000)
    struct.pack_into('<I', data, 152 + 60, 512)
    struct.pack_into('<I', data, 152 + 92, 16)
    index = 13 if delayed else 1
    struct.pack_into('<II', data, 152 + 96 + index * 8, 0x1000, 64 if delayed else 40)
    struct.pack_into('<IIII', data, 376 + 8, 1536, 0x1000, 1536, 512)
    if delayed:
        struct.pack_into('<II', data, 512, 1, 0x1100)
    else:
        struct.pack_into('<I', data, 512 + 12, 0x1100)
    name = import_name.encode() + b'\0'
    data[768:768 + len(name)] = name
    return bytes(data)


class ImportTests(unittest.TestCase):
    def test_import_and_delay_import_are_dependency_edges(self):
        for delayed in (False, True):
            with self.subTest(delayed=delayed):
                self.assertEqual(audit.pe_info(pe(delayed=delayed))['imports'], ['SDL3.dll'])
                info = audit.pe_info(pe('SDL2.dll', delayed=delayed))
                with self.assertRaisesRegex(ValueError, 'SDL2'):
                    audit.require_native_sdl3('consumer.dll', info)

    def test_delay_import_legacy_va_name(self):
        data = bytearray(pe(delayed=True))
        struct.pack_into('<II', data, 512, 0, 0x401100)
        self.assertEqual(audit.pe_info(data)['imports'], ['SDL3.dll'])

    def test_x64_machine_and_malformed_rva_rejected(self):
        with self.assertRaisesRegex(ValueError, 'x86'):
            audit.require_native_sdl3('provider.dll', audit.pe_info(pe(machine=0x8664)))
        data = bytearray(pe())
        struct.pack_into('<I', data, 512 + 12, 0x9000)
        with self.assertRaisesRegex(ValueError, 'RVA'):
            audit.pe_info(data)
        with self.assertRaises(ValueError):
            audit.pe_info(b'MZ')

    def test_source_consumers_reject_old_includes_and_compatibility(self):
        for source in ['#include "../external/sdl2/SDL.h"', '#include <SDL.h>',
                       '#define SDL_ENABLE_OLD_NAMES 1', 'LDFLAGS=-lSDL2',
                       '<AdditionalDependencies>SDL2.lib;</AdditionalDependencies>']:
            with self.subTest(source=source), self.assertRaises(ValueError):
                audit.check_source('consumer.cpp', source)
        audit.check_source('consumer.cpp', '// Preserve SDL2 behavior\n#include <SDL3/SDL.h>')

    def test_runtime_audits_transitive_and_unreferenced_modules(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'hl.exe').write_bytes(pe())
            shutil.copyfile(PINNED_DLL, root / 'SDL3.dll')
            report = audit.audit_runtime(root)
            self.assertEqual(len(report['binaries']), 2)
            (root / 'old.dll').write_bytes(pe('SDL2.dll', delayed=True))
            with self.assertRaisesRegex(ValueError, 'SDL2'):
                audit.audit_runtime(root)
            (root / 'old.dll').write_bytes(pe('missing_provider.dll'))
            with self.assertRaisesRegex(ValueError, 'missing_provider'):
                audit.audit_runtime(root)

    def test_fixture_requires_explicit_decisions_for_every_native_overlay(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            data = root / 'data'
            runtime = root / 'runtime'
            data.mkdir()
            runtime.mkdir()
            (data / 'SDL2.dll').write_bytes(pe('KERNEL32.dll'))
            (data / 'mss32.dll').write_bytes(pe('KERNEL32.dll'))
            (data / 'pak0.pak').write_bytes(b'licensed data')
            for relative in audit.REQUIRED_RUNTIME:
                path = runtime / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(pe())
            shutil.copyfile(PINNED_DLL, runtime / 'SDL3.dll')
            with self.assertRaisesRegex(ValueError, 'decision'):
                audit.prepare_fixture(data, runtime, root / 'fixture', {})
            decisions = {'providers': ['mss32.dll'], 'excluded': {'SDL2.dll': 'replaced by pinned native SDL3'}}
            report = audit.prepare_fixture(data, runtime, root / 'fixture', decisions)
            self.assertTrue((root / 'fixture/mss32.dll').exists())
            self.assertTrue((root / 'fixture/pak0.pak').exists())
            self.assertFalse((root / 'fixture/SDL2.dll').exists())
            self.assertEqual(len(report['excluded']), 1)
            self.assertEqual(report['runtime_hashes']['hl.exe'], audit.sha256(runtime / 'hl.exe'))

    def test_runtime_requires_pinned_sdl3_provider(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'SDL3.dll').write_bytes(pe('KERNEL32.dll'))
            with self.assertRaisesRegex(ValueError, 'pinned SDL3'):
                audit.audit_runtime(root)

    def test_same_basename_in_unsearched_subdirectory_is_not_provider_closure(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            shutil.copyfile(PINNED_DLL, root / 'SDL3.dll')
            (root / 'hl.exe').write_bytes(pe('required.dll'))
            (root / 'unrelated').mkdir()
            (root / 'unrelated/required.dll').write_bytes(pe('KERNEL32.dll'))
            with self.assertRaisesRegex(ValueError, 'required.dll'):
                audit.audit_runtime(root)

    def test_missing_game_data_cannot_become_successful_runtime_only_fixture(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            runtime = root / 'runtime'
            runtime.mkdir()
            for relative in audit.REQUIRED_RUNTIME:
                path = runtime / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(pe())
            shutil.copyfile(PINNED_DLL, runtime / 'SDL3.dll')
            with self.assertRaisesRegex(ValueError, 'data/runtime'):
                audit.prepare_fixture(root / 'missing-data', runtime, root / 'fixture', {})

    def test_export_forwarder_is_a_dependency_edge(self):
        data = bytearray(pe('KERNEL32.dll'))
        struct.pack_into('<II', data, 152 + 96, 0x1200, 0x200)
        struct.pack_into('<I', data, 1024 + 20, 1)
        struct.pack_into('<I', data, 1024 + 28, 0x1300)
        struct.pack_into('<I', data, 1280, 0x1280)
        name = b'SDL2.SDL_Init\0'
        data[1152:1152 + len(name)] = name
        with self.assertRaisesRegex(ValueError, 'SDL2'):
            audit.require_native_sdl3('forwarder.dll', audit.pe_info(data))

    def test_exports_ordinals_and_resource_bytes_are_recorded(self):
        data = bytearray(pe('KERNEL32.dll'))
        struct.pack_into('<II', data, 152 + 96, 0x1200, 0x200)
        struct.pack_into('<IIIII', data, 1024 + 16, 7, 2, 1, 0x1300, 0x1310)
        struct.pack_into('<I', data, 1024 + 36, 0x1320)
        struct.pack_into('<II', data, 1280, 0x1500, 0x1501)
        struct.pack_into('<I', data, 1296, 0x1380)
        struct.pack_into('<H', data, 1312, 0)
        name = b'CreateInterface\0'
        data[1408:1408 + len(name)] = name
        payload = b'resource fixture'
        struct.pack_into('<II', data, 152 + 96 + 2 * 8, 0x1400, len(payload))
        data[1536:1536 + len(payload)] = payload
        info = audit.pe_info(data)
        self.assertIn('exports', info)
        self.assertEqual([(entry['ordinal'], entry['name']) for entry in info['exports']],
                         [(7, 'CreateInterface'), (8, None)])
        import hashlib
        self.assertEqual(info['resource_directory_sha256'], hashlib.sha256(payload).hexdigest())
        struct.pack_into('<H', data, 1312, 2)
        with self.assertRaisesRegex(ValueError, 'ordinal'):
            audit.pe_info(data)

    def test_exact_os_modules_keep_chrome_and_directdraw_provider_closure(self):
        for name in ('WINSPOOL.DRV', 'winspool.drv', 'DDRAW.dll', 'ddraw.DLL'):
            with self.subTest(name=name):
                self.assertTrue(audit.is_system_dll(name))
        for name in ('CUSTOM.DRV', 'DDRAW.DRV', 'WINSPOOL-CUSTOM.DRV', 'MSVCP140.dll', 'VCRUNTIME140.dll'):
            with self.subTest(name=name):
                self.assertFalse(audit.is_system_dll(name))
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            shutil.copyfile(PINNED_DLL, root / 'SDL3.dll')
            (root / 'libcef.dll').write_bytes(pe('WINSPOOL.DRV'))
            (root / 'swds.dll').write_bytes(pe('DDRAW.dll'))
            report = audit.audit_runtime(root)
            self.assertIn('libcef.dll', report['binaries'])
            self.assertIn('swds.dll', report['binaries'])
            self.assertIn('WINSPOOL.DRV', report['system_imports'])
            self.assertIn('DDRAW.dll', report['system_imports'])
            (root / 'unknown.dll').write_bytes(pe('CUSTOM.DRV'))
            with self.assertRaisesRegex(ValueError, 'CUSTOM.DRV'):
                audit.audit_runtime(root)
