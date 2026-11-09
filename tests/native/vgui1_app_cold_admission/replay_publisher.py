#!/usr/bin/env python3
"""Compile the constant publisher under each exact original owner profile; never link/load it."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
import engine_bootstrap_evidence as common
import vgui1_app_cold_manifest as manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    output = options.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    receipt, _ = manifest.fresh_receipt(options.evidence.resolve())
    runs = []
    source = ROOT / 'GoldSrc-SDK/common/vgui1_app_cold_publisher.cpp'
    for context in receipt['contexts']:
        role = 'OFFER' if context['role'] == 'provider' else 'REQUIREMENT'
        image = output / (context['name'] + '.o')
        command = common.compilation_prefix(context) + ['-DCSE_VGUI1_APP_COLD_' + role + '=1',
            '-DCSE_VGUI1_APP_COLD_GENERATED_HEADER="' + str(options.evidence.resolve() / 'metadata.h') + '"',
            '-c', str(source), '-o', str(image)]
        process = subprocess.run(command, cwd=context['directory'], capture_output=True, timeout=45)
        (output / (context['name'] + '.log')).write_bytes(process.stdout + process.stderr)
        runs.append({'command': command, 'cwd': context['directory'], 'exit': process.returncode})
        if process.returncode != 0:
            raise RuntimeError(process.stderr.decode(errors='replace'))
        load_commands = subprocess.check_output(['/usr/bin/otool', '-l', str(image)])
        (output / (context['name'] + '-load-commands.txt')).write_bytes(load_commands)
        if load_commands.count(b'sectname __cse_vgui1') != 1:
            raise RuntimeError('Publisher must have exactly one constant data record')
        symbols = subprocess.check_output(['/usr/bin/nm', '-nm', str(image)])
        (output / (context['name'] + '-symbols.txt')).write_bytes(symbols)
        inherited = b'__mod_init_func' in load_commands
        if inherited and b'CseSteamModuleInitialize' not in symbols:
            raise RuntimeError('Unclassified initializer in actual publisher recipe')
        llvm = output / (context['name'] + '.ll')
        ir_command = command[:-4] + ['-S', '-emit-llvm', str(source), '-o', str(llvm)]
        ir = subprocess.run(ir_command, cwd=context['directory'], capture_output=True, timeout=45)
        if ir.returncode != 0 or 'CseVgui1AppColdMetadata = hidden constant' not in llvm.read_text():
            raise RuntimeError('Actual compiler must emit metadata as constant initialized storage')
        runs[-1]['llvm_command'] = ir_command
        runs[-1]['inherited_target_initializer'] = 'SteamModuleInitialize' if inherited else None
        runs[-1]['object_sha256'] = hashlib.sha256(image.read_bytes()).hexdigest()
        runs[-1]['metadata_dynamic_initialization'] = False
    manifest.fresh_receipt(options.evidence.resolve())
    (output / 'commands.json').write_text(json.dumps(runs, indent=2))
    print('PASS three exact owner profiles, constant metadata; inherited Steam initialization recorded separately')


if __name__ == '__main__':
    main()
