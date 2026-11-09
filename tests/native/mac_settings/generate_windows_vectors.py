"""Generate C++14 controlled vectors from the actual UTF8-activeCodePage probe."""
import argparse
import hashlib
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    raw = args.input.read_bytes()
    text = raw.decode('ascii')
    if not text.startswith('ACP=65001\n') or not text.endswith('owned_registry_cleanup=0\n'):
        text = raw.decode('ascii').replace('\r\n', '\n')
    assert text.startswith('ACP=65001\n') and text.endswith('owned_registry_cleanup=0\n')
    inputs = ['', 'A', 'AB', 'ABC', 'ABCD', 'ABCDE', 'é', 'Ж', '🙂', 'AéZ']
    rows = re.findall(r'^case=(\d+) capacity=(\d+) code=(\d+) required=(\d+) bytes=([0-9a-f]{40})$', text, re.M)
    assert len(rows) == len(inputs) * 17
    out = ['// Generated from the exact owned Win32 App/ACP65001 probe; do not edit vectors.',
           '// Source log SHA256: ' + hashlib.sha256(raw).hexdigest(),
           'struct StringQueryVector {', '    unsigned int input;', '    std::size_t capacity;',
           '    bool success;', '    std::size_t required;', '    std::array<unsigned char, 20> bytes;', '};',
           'constexpr const char* StringInputs[] = {']
    out += ['    "' + ''.join('\\x' + format(byte, '02x') for byte in value.encode('utf-8')) + '",' for value in inputs]
    out += ['};', 'constexpr StringQueryVector StringQueries[] = {']
    for index, capacity, code, required, data in rows:
        assert int(code) in (0, 234)
        values = ', '.join('0x' + data[offset:offset + 2] for offset in range(0, len(data), 2))
        out.append('    {' + f'{index}, {capacity}, {"true" if code == "0" else "false"}, {required}, {{{{' +
                   values + '}}},')
    out += ['};']
    args.output.write_text('\n'.join(out) + '\n')


if __name__ == '__main__':
    main()
