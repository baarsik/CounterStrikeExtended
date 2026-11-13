#!/usr/bin/env python3
"""Read-only actual CS client compiler replay and private proposed-policy controls."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
import engine_bootstrap_evidence as common


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--database-root', type=Path, required=True)
    options = parser.parse_args()
    output = options.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    source = ROOT / 'src/cstrike/cl_dll/ev_hldm.cpp'
    original = source.read_bytes()
    (output / 'ev_hldm-before.cpp').write_bytes(original)
    runs = []
    for profile in ('release', 'debug'):
        work = output / profile
        work.mkdir()
        database = options.database_root / ('macos-runtime-policy-' + profile) / 'compile_commands.json'
        rows = json.loads(database.read_text())
        matches = [row for row in rows if Path(row['file']).resolve() == source
                   and '/CMakeFiles/cs_client.dir/' in row['output']]
        common.require(len(matches) == 1, 'Exactly one actual CS client owner required')
        row = matches[0]
        context = {'name': profile, 'role': 'consumer', 'source': str(source),
                   'directory': row['directory'], 'arguments': row.get('arguments') or shlex.split(row['command']),
                   'microsoft': False,
                   'filesystem_abi_header': str(ROOT / 'src/public/cse_module_abi.h')}
        prefix = common.compilation_prefix(context)
        inputs = {str(source): common.file_digest(source), str(database): common.file_digest(database),
                  str(Path(__file__).resolve()): common.file_digest(__file__)}
        identities, configurations = common.compiler_identity(context, prefix, work)
        for item in configurations:
            inputs[str(item.resolve())] = common.file_digest(item)
        depfile = work / 'before.d'
        preprocess = prefix + ['-E', '-MD', '-MF', str(depfile), '-MT', str(work / 'before.o'), str(source)]
        pp = subprocess.run(preprocess, cwd=context['directory'], capture_output=True, timeout=60)
        (work / 'preprocess.log').write_bytes(pp.stdout + pp.stderr)
        common.require(pp.returncode == 0, 'Actual headers must preprocess before compiler RED')
        closure = common.dependency_paths(context, pp.stdout + pp.stderr, depfile, source)
        inputs.update({str(path): common.file_digest(path) for path in closure})
        common.verify_inputs(inputs)
        common.verify_inputs(identities)
        command = prefix + ['-c', str(source), '-o', str(work / 'actual.o')]
        process = subprocess.run(command, cwd=context['directory'], capture_output=True, timeout=60)
        (work / 'compile.log').write_bytes(process.stdout + process.stderr)
        common.verify_inputs(inputs)
        common.verify_inputs(identities)
        runs.append({'profile': profile, 'command': command, 'cwd': context['directory'], 'exit': process.returncode,
                     'preprocess_command': preprocess, 'preprocess_exit': pp.returncode,
                     'inputs': inputs, 'compiler_identities': identities,
                     'error_lines': [line for line in process.stderr.decode(errors='replace').splitlines()
                                     if 'error:' in line]})
    common.require(source.read_bytes() == original, 'Production source changed during audit')
    (output / 'commands.json').write_text(json.dumps(runs, indent=2))
    for row in runs:
        print(row['profile'], row['exit'], row['error_lines'])


if __name__ == '__main__':
    main()
