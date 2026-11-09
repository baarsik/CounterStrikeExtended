"""Analyze exported metadata/digests only; never opens a font or raster byte file."""
import argparse
import hashlib
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--packet', required=True, type=Path)
parser.add_argument('--prior-fixedsys', required=True, type=Path)
args = parser.parse_args()
profiles = ['x86-Release', 'x86-Debug', 'x64-Release', 'x64-Debug']
records = {}
first = None
undefined = {0x81, 0x8d, 0x8f, 0x90, 0x9d}
defined_graphic_positions = set(range(32, 256)) - undefined - {0x7f}
for profile in profiles:
    path = args.packet / 'windows-v5' / profile / 'cells.tsv'
    data = path.read_bytes()
    rows = [line.split('\t') for line in data.decode().splitlines()]
    compared = {int(row[1]): row for row in rows if row[0] == 'compare'}
    facts = {int(row[1]): row for row in rows if row[0] == 'gdi-facts'}
    ft = {int(row[2]): row for row in rows if row[:2] == ['ft-cell', 'raw-byte']}
    assert set(compared) == set(facts) == set(ft) == set(range(256))
    assert all(row[2] == 'ABC=0/8/0' for row in facts.values())
    assert all(row[5] == '512' and row[10] == '0' for row in ft.values())
    prior = {int(row[0]): row[-1] for line in
             (args.prior_fixedsys / 'windows' / profile / 'cells.tsv').read_text().splitlines()
             if (row := line.split('\t'))}
    assert all(compared[byte][3] == prior[byte] for byte in range(256))
    raw_mismatch = [byte for byte, row in compared.items() if row[4] == 'raw-equal=0']
    unicode_mismatch = [byte for byte, row in compared.items() if row[5] == 'unicode-equal=0']
    assert not (set(raw_mismatch) & defined_graphic_positions)
    assert set(raw_mismatch) == (set(range(32)) - {9, 12}) | undefined
    assert set(unicode_mismatch) == set(raw_mismatch) | {0x91, 0x92}
    record = {
        'cells_sha256': hashlib.sha256(data).hexdigest(),
        'prior_all256_gdi_digest_matches': True,
        'all256_ABC': [0, 8, 0],
        'all256_ft_advance26_6': 512,
        'ft_ink_cropped': False,
        'raw_byte_matches': 256 - len(raw_mismatch),
        'defined_graphic_positions_count': len(defined_graphic_positions),
        'defined_graphic_positions_all_match': True,
        'raw_byte_mismatch': raw_mismatch,
        'raw_mismatch_blank_gdi': [byte for byte in raw_mismatch if facts[byte][3] == 'nonzero=0'],
        'raw_mismatch_nonblank_gdi': [byte for byte in raw_mismatch if facts[byte][3] != 'nonzero=0'],
        'unicode_negative_control_mismatch': unicode_mismatch,
        'gdi_default128_facts': facts[128],
        'gdi_space32_facts': facts[32],
        'ft_default0_hash': ft[0][-1],
        'gdi_default128_hash': compared[128][3],
    }
    assert record['ft_default0_hash'] == record['gdi_default128_hash']
    if first is None:
        first = data
    assert first == data
    records[profile] = record
result = {'scope': 'Metadata and SHA comparisons only; bitmap bytes remain inside VM1',
          'profiles': records, 'all_four_cells_files_identical': True}
(args.packet / 'analysis.json').write_text(json.dumps(result, indent=2) + '\n')
print('4 profiles: all256 prior GDI hashes unchanged; 221/256 WinFNT matches; all218 defined graphic positions match')
