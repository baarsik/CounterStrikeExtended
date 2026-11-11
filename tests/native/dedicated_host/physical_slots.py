import importlib.util
import pathlib
import subprocess
import sys
import tempfile
root = pathlib.Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('slots', root / 'tests/native/engine_bootstrap_abi/check_slots.py')
slots = importlib.util.module_from_spec(spec)
spec.loader.exec_module(slots)
header = (root / 'GoldSrc-SDK/public/dedicated_api.h').read_text()
header = header.replace('#pragma once', '').replace('#include "interface_registry.h"',
    '#include "' + str(root / 'GoldSrc-SDK/public/interface_registry.h') + '"')
probe = '''
bool Frame(IDedicatedServerAPI* p) { return p->RunFrame(); }
int Stop(IDedicatedServerAPI* p) { return p->Shutdown(); }
'''
with tempfile.TemporaryDirectory() as directory:
    for reorder in (False, True):
        text = header
        if reorder:
            text = text.replace('virtual int Shutdown() = 0;\n    virtual bool RunFrame() = 0;', 'virtual bool RunFrame() = 0;\n    virtual int Shutdown() = 0;')
        source = pathlib.Path(directory) / 'probe.cpp'
        source.write_text(text + probe)
        run = subprocess.run(['clang++', '-std=c++17', '-Xclang', '-fdump-vtable-layouts',
                              '-S', '-emit-llvm', str(source), '-o', str(source.with_suffix('.ll'))],
                             capture_output=True, text=True, check=True)
        facts = slots.facts(run.stdout + run.stderr, False)
        expected = ['destructor', 'destructor', 'Init', 'Shutdown', 'RunFrame', 'AddConsoleText', 'UpdateStatus']
        observed = next(f['slots'] for f in facts if f['type'] == 'IDedicatedServerAPI')
        matches = observed == [{'slot': i, 'method': method} for i, method in enumerate(expected)]
        assert matches != reorder, observed
