#!/usr/bin/env python3
"""Dedicated production owning-build physical ABI gate; no generic cold-load promotion."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('bootstrap_slots', ROOT / 'tests/native/engine_bootstrap_abi/check_slots.py')
slots = importlib.util.module_from_spec(spec)
spec.loader.exec_module(slots)
slots.METHODS['IGameServerData'] = ['WriteDataRequest', 'ReadDataResponse']
PROBES = '''
extern "C" bool DedicatedEvidenceInit(IDedicatedServerAPI* p, char* b, char* c, CreateInterfaceFn h, CreateInterfaceFn f) { return p->Init(b,c,h,f); }
extern "C" int DedicatedEvidenceShutdown(IDedicatedServerAPI* p) { return p->Shutdown(); }
extern "C" bool DedicatedEvidenceFrame(IDedicatedServerAPI* p) { return p->RunFrame(); }
extern "C" void DedicatedEvidenceConsole(IDedicatedServerAPI* p, char* c) { p->AddConsoleText(c); }
extern "C" void DedicatedEvidenceStatus(IDedicatedServerAPI* p, float* f, int* a, int* m, char* n) { p->UpdateStatus(f,a,m,n); }
extern "C" void DedicatedEvidencePrint(IDedicatedExports* p, char* t) { p->Sys_Printf(t); }
extern "C" void DedicatedEvidenceWrite(IGameServerData* p, const void* b, int n) { p->WriteDataRequest(b,n); }
extern "C" int DedicatedEvidenceRead(IGameServerData* p, void* b, int n) { return p->ReadDataResponse(b,n); }
extern "C" IBaseInterface* DedicatedEvidenceApiBase(IDedicatedServerAPI* p) { return p; }
extern "C" IBaseInterface* DedicatedEvidenceExportBase(IDedicatedExports* p) { return p; }
extern "C" IBaseInterface* DedicatedEvidenceAdminBase(IGameServerData* p) { return p; }
static_assert(sizeof(int) == 4 && alignof(int) == 4, "Dedicated admin envelope words remain 32-bit");
'''

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--database', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    options.output = options.output.resolve()
    options.output.mkdir(parents=True, exist_ok=True)
    stamp = options.output / 'accepted.json'
    stamp.unlink(missing_ok=True)
    rows = json.loads(options.database.read_text())
    contexts = [('swds', '/engine/sys_dll2.cpp'), ('swds', '/engine/sv_remoteaccess.cpp'),
                ('hlds', '/dedicated/native/DedicatedSession.cpp')]
    receipts = []
    for target, suffix in contexts:
        selected = [r for r in rows if r['file'].endswith(suffix)
                    and f'targets/{target}/' in r['command']]
        if len(selected) != 1:
            raise RuntimeError(f'Expected one real owning compile for {target} {suffix}, got {len(selected)}')
        row = selected[0]
        label = target + '-' + Path(row['file']).stem
        source = options.output / (label + '.cpp')
        source.write_text('#include "' + row['file'] + '"\n#include "'
                          + str(ROOT / 'GoldSrc-SDK/public/native_dedicated_abi.h') + '"\n' + PROBES)
        args = shlex.split(row['command'])
        if any(arg.startswith('@') for arg in args):
            raise RuntimeError('Unqualified response-file compile policy; refusing stale/partial evidence')
        clean = []
        skip = False
        for arg in args:
            if skip:
                skip = False
                continue
            if arg in ['-o', '-MF', '-MT', '-MQ']:
                skip = True
            elif arg not in ['-MD', '-MMD', '-c', row['file']]:
                clean.append(arg)
        command = clean + ['-DCSE_DEDICATED_EVIDENCE_PROBE=1', '-Xclang', '-fdump-vtable-layouts',
                           '-Xclang', '-fdump-record-layouts', '-S', '-emit-llvm', str(source),
                           '-o', str(options.output / (label + '.ll'))]
        result = subprocess.run(command, cwd=row['directory'], capture_output=True, timeout=180)
        log = options.output / (label + '.log')
        log.write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(f'Actual owning compile failed: {log}')
        facts = {r['type'].split('::')[-1]: r for r in slots.facts(log.read_text(errors='replace'), False)}
        for name in ['IDedicatedServerAPI', 'IDedicatedExports', 'IGameServerData']:
            expected = ['destructor', 'destructor'] + slots.METHODS[name]
            actual = facts.get(name, {}).get('slots')
            if actual != [{'slot': i, 'method': method} for i, method in enumerate(expected)]:
                raise RuntimeError(f'{label}: wrong physical slots for {name}: {actual}')
            records = log.read_text(errors='replace').split('*** Dumping AST Record Layout\n')
            record = next((part.split('\n\n')[0] for part in records
                           if re.match(r'\s*0 \| class ' + name + r'\n', part)), None)
            if (not record or not re.search(r'0 \|   class IBaseInterface \(primary base\)', record)
                    or '[sizeof=8, dsize=8, align=8,' not in record):
                raise RuntimeError(f'{label}: wrong native object/base record for {name}')
        preprocessed = subprocess.run(clean + ['-E', '-P', str(source)], cwd=row['directory'],
                                      capture_output=True, timeout=180)
        if preprocessed.returncode:
            raise RuntimeError('Cannot bind owning include closure')
        compiler = subprocess.run([clean[0], '--version'], capture_output=True, check=True).stdout
        receipts.append(dict(owner=row, source_sha256=digest(Path(row['file'])), command=command,
                             compiler_sha256=digest(Path(clean[0])), compiler_version=compiler.decode(),
                             preprocessed_sha256=hashlib.sha256(preprocessed.stdout).hexdigest(),
                             physical_slots=facts, log_sha256=digest(log)))
    stamp.write_text(json.dumps({'scope': 'dedicated API/exports/admin roles only',
        'preload': 'separate MacRuntimeSession exact software/VGUI cohort, before constructors',
        'filesystem': 'separate filesystem009 typed descriptor before factory',
        'generic_bootstrap_promoted': False, 'receipts': receipts}, indent=2))
    print('Accepted actual dedicated physical slots/base/envelope evidence:', len(receipts), 'owning contexts')

if __name__ == '__main__':
    main()
