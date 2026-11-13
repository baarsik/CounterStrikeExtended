"""Extract exact pinned V11 writer expressions and generate binary64 clock controls."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct


def prepare(source, destination):
    cpp = source / 'src/HLTV/common/DemoFile.cpp'
    header = source / 'src/HLTV/common/DemoFile.h'
    text = cpp.read_text()
    begin = text.index('bool DemoFile::StartRecording(')
    end = text.index('bool DemoFile::', begin + 1)
    section = text[begin:end]
    start = re.findall(r'^\s*(startTime = \(float\)m_System->GetTime\(\);)', section, re.M)
    elapsed = re.findall(r'^\s*(f = \(float\)\( m_System->GetTime\(\) - startTime \);)', section, re.M)
    assert len(start) == 1 and len(elapsed) >= 2, (start, elapsed)
    assert len(set(elapsed)) == 1
    assert re.search(r'float\s+startTime\s*;', header.read_text())
    destination.mkdir(parents=True, exist_ok=True)
    body = ('void WriterExpressions::Start() {\n    ' + start[0] + '\n}\n\n'
            'float WriterExpressions::EncodeTime() {\n    float f;\n    ' + elapsed[0] + '\n    return f;\n}\n')
    (destination / 'writer_expressions.inc').write_text(body)
    clocks = []
    for value in [1.0, 1.5, 2.0, 10.0, 60.0, 1024.0]:
        for name, sample in [('below', math.nextafter(value, -math.inf)), ('exact', value),
                             ('above', math.nextafter(value, math.inf))]:
            clocks.append((f'{int(value * 10)}_{name}', sample))
    clocks.extend([('binary32_midpoint_up', 2.0 - 2.0 ** -24),
                   ('binary32_midpoint_down', 2.0 + 2.0 ** -23),
                   ('observed_residue', 1.5 + struct.unpack('<f', struct.pack('<f', -3.7679036779536546e-08))[0])])
    rows = []
    declarations = []
    for name, value in clocks:
        bits = struct.unpack('<Q', struct.pack('<d', value))[0]
        next_bits = struct.unpack('<Q', struct.pack('<d', value + 0.25))[0]
        declarations.append(f'    {{"{name}", 0x{bits:016x}ULL, 0x{next_bits:016x}ULL}},')
        start32 = struct.unpack('<f', struct.pack('<f', value))[0]
        first32 = struct.unpack('<I', struct.pack('<f', value - start32))[0]
        advanced32 = struct.unpack('<I', struct.pack('<f', value + 0.25 - start32))[0]
        rows.append({'name': name, 'clock_bits': f'{bits:016x}', 'next_bits': f'{next_bits:016x}',
                     'start_bits': f"{struct.unpack('<I', struct.pack('<f', start32))[0]:08x}", 'first_bits': f'{first32:08x}',
                     'advanced_bits': f'{advanced32:08x}'})
    (destination / 'clock_cases.inc').write_text('struct ClockCase {\n    const char *name;\n    std::uint64_t clockBits;\n    std::uint64_t nextBits;\n};\n'
        'constexpr ClockCase ClockCases[] = {\n' + '\n'.join(declarations) + '\n};\n')
    receipt = {'source': str(source), 'files': [{'relative': str(path.relative_to(source)),
                'sha256': hashlib.sha256(path.read_bytes()).hexdigest()} for path in [cpp, header]],
               'actual_start_statement': start[0], 'actual_elapsed_statement': elapsed[0],
               'start_type': 'float', 'clock_type': 'virtual double GetTime()', 'clock_cases': rows}
    (destination / 'extraction.json').write_text(json.dumps(receipt, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--destination', type=Path, required=True)
    args = parser.parse_args()
    prepare(args.source, args.destination)
