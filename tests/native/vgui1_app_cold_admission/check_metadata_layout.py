#!/usr/bin/env python3
"""Independent byte/section/export controls for actual normal and sanitized images."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--debug-build', type=Path, required=True)
    parser.add_argument('--sanitized-build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    rows = []
    records = {}
    for profile, build in [('debug', options.debug_build.resolve()), ('sanitized', options.sanitized_build.resolve())]:
        for image, kind in [('libcold_root.dylib', 2), ('libcold_vgui.dylib', 1)]:
            path = build / image
            facts = json.loads(subprocess.check_output([str(build / 'app_cold_inspect'), str(path)]))
            expected = struct.pack('<Q6I', 0x3130444341455343, 1, 352, kind, 0x4f4d0206, 0, 0)
            expected += bytes.fromhex(facts['provenance']) + struct.pack('<8I', *facts['catalog'])
            for table in facts['tables']:
                expected += struct.pack('<IIQ', *table)
            expected += b'\0' * ((16 - len(facts['tables'])) * 16)
            raw = path.read_bytes()
            record = raw[facts['metadata_offset']:facts['metadata_offset'] + 352]
            assert len(expected) == 352 and record == expected
            load = subprocess.check_output(['/usr/bin/otool', '-l', str(path)]).decode()
            sizes = re.findall(r'sectname __cse_vgui1\s+segname __DATA_CONST\s+addr \S+\s+size (\S+)', load)
            assert len(sizes) == 1 and int(sizes[0], 16) == 352
            exports = subprocess.check_output(['/usr/bin/nm', '-gU', str(path)])
            symbols = subprocess.check_output(['/usr/bin/nm', '-nm', str(path)])
            assert b'CseVgui1AppColdMetadata' not in exports
            assert b'___asan_global_CseVgui1AppColdMetadata' not in symbols
            assert b'asan.module_ctor' in symbols if profile == 'sanitized' else True
            records[(profile, kind)] = record
            rows.append({'profile': profile, 'path': str(path), 'kind': kind, 'section_bytes': 352,
                         'all_encoded_field_bytes_match': True, 'metadata_exported': False,
                         'metadata_asan_redzone': False, 'sha256': hashlib.sha256(raw).hexdigest(),
                         'record_sha256': hashlib.sha256(record).hexdigest()})
    for kind in (1, 2):
        assert records[('debug', kind)] == records[('sanitized', kind)]
    options.output.write_text(json.dumps({'images': rows, 'debug_sanitized_exact_record_equality': True,
        'scope': 'serialized array layout exemption only; all owners/parser remain instrumented'}, indent=2))
    print('PASS exact352 bytes, independently encoded fields, hidden symbol, Debug/sanitized record equality')


if __name__ == '__main__':
    main()
