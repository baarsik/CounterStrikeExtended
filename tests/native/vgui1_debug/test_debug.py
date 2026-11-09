"""Owned pipe byte/return controls. No GUI, provider or settings operations."""
import argparse
import errno
import json
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--native', type=Path, required=True)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args()
    args.evidence.mkdir(parents=True, exist_ok=True)
    observations = []

    def run(binary, mode, *arguments):
        # communicate() concurrently drains both pipes; the >512KiB case cannot block on pipe capacity.
        result = subprocess.run([str(binary), mode, *map(str, arguments)], capture_output=True, timeout=30)
        assert result.returncode == 0, (binary, mode, result.returncode, result.stdout, result.stderr)
        tag = ('native' if binary == args.native else 'reference') + '-' + mode
        if arguments:
            tag += '-' + '-'.join(map(str, arguments))
        (args.evidence / (tag + '.stderr')).write_bytes(result.stderr)
        (args.evidence / (tag + '.stdout')).write_bytes(result.stdout)
        observations.append({'case': tag, 'exit': result.returncode, 'stderr_bytes': len(result.stderr),
                             'returns': result.stdout.decode('ascii').splitlines()})
        return result.stdout, result.stderr

    for mode in ['plain', 'counter']:
        for count in [0, 1, 2044, 2045, 2046, 2047, 2048, 4096]:
            native = run(args.native, mode, count)
            reference = run(args.reference, mode, count)
            assert native == reference
            prefix = b'' if mode == 'plain' else b'0:'
            assert native == (f'{count}\n'.encode(), prefix + b'x' * min(count, 2047 - len(prefix)))
    for mode in ['values', 'nul', 'sequence', 'prefix-limit', 'error']:
        native = run(args.native, mode)
        assert native == run(args.reference, mode), mode
        if mode == 'values':
            expected = b'[-17][23][4294967297][31][1.25][tail]'
            assert native == (str(len(expected)).encode() + b'\n', expected)
        elif mode == 'nul':
            assert native == (b'3\n3\n', b'A0:C')
        elif mode == 'sequence':
            assert native[1] == b''.join(f'{index}:[{index}]'.encode() for index in range(12))
        elif mode == 'prefix-limit':
            assert native[1] == b''.join(f'{index}:'.encode() + b'p' * (2047 - len(f'{index}:')) for index in range(12))
        else:
            lines = native[0].splitlines()
            assert int(lines[0]) < 0 and int(lines[2]) < 0
            assert lines[1].startswith(b'errno=') and int(lines[1][6:]) != 0
            assert int(lines[1][6:]) == int(lines[3][6:]) == errno.EILSEQ
            assert native[1] == b'0:'
    positive = run(args.native, 'wrap-positive')
    assert positive == (b'1\n' * 4, b'2147483646:!2147483647:!-2147483648:!-2147483647:!')
    limit = run(args.native, 'wrap-limit')
    assert limit == (b'4096\n', b'-2147483648:' + b'w' * (2047 - len(b'-2147483648:')))
    zero = run(args.native, 'wrap-zero')
    assert zero == (b'1\n' * 4, b'-2:!-1:!0:!1:!')
    stdout, output = run(args.native, 'concurrent')
    assert stdout == b'plain=1024 counter=1024\n'
    assert len(output) > 16 * 1024
    records = re.findall(rb'(?:(\d+):C|P)\[(\d+),(\d+)\](z{256});', output)
    rebuilt = b''.join((counter + b':C' if counter else b'P') + b'[' + thread + b',' + index + b']' + payload + b';'
                       for counter, thread, index, payload in records)
    assert rebuilt == output, 'interleaved or lost stderr records'
    plain = {(int(thread), int(index)) for counter, thread, index, _ in records if not counter}
    counted = [(int(counter), int(thread), int(index)) for counter, thread, index, _ in records if counter]
    assert len(plain) == len(counted) == 1024
    assert plain == {(thread, index) for thread in range(8) for index in range(128)}
    assert {(thread, index) for _, thread, index in counted} == plain
    assert [counter for counter, _, _ in counted] == list(range(1024)), 'counter/emission order differs'
    report = {'status': 'passed', 'pipe_cases': len(observations), 'observations': observations,
              'scope': 'same native formatter source control; no arbitrary MSVCRT extension claim'}
    (args.evidence / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print('PASS real stderr bytes/returns, bounded output, NUL/error, signed counter wrap and2048 concurrent records')


if __name__ == '__main__':
    main()
