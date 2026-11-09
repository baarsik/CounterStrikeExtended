#!/usr/bin/env python3
"""Pin exact production interface declarations in isolated, named test contexts."""
import argparse
import hashlib
import json
from pathlib import Path


def declarations(path, name):
    text = path.read_text(encoding='latin-1')
    marker = 'class ' + name + ' : public IBaseInterface'
    result = []
    start = 0
    while True:
        position = text.find(marker, start)
        if position < 0:
            return result
        start = text.index('\n};', position) + len('\n};')
        result.append(text[position:start])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    options.output.mkdir(parents=True, exist_ok=True)
    sdk = options.root / 'GoldSrc-SDK'
    contexts = [
        ('EngineProducer', 'engine/sys_dll2.cpp', ['IEngineAPI', 'IDedicatedServerAPI', 'IDedicatedExports']),
        ('LauncherCaller', 'launcher/public/engine_launcher_api.h', ['IEngineAPI']),
        ('DedicatedCaller', 'dedicated/sys_ded.cpp', ['IDedicatedServerAPI']),
        ('DedicatedExportsCaller', 'public/idedicatedexports.h', ['IDedicatedExports'])]
    lines = ['#pragma once', '#include "' + str(sdk / 'public/interface_registry.h') + '"',
             '// Exact source-extracted declarations. Controlled bindings; no replacement production class.']
    receipt = []
    for namespace, source, names in contexts:
        path = sdk / source
        lines.append('namespace ' + namespace + ' {')
        for name in names:
            found = declarations(path, name)
            assert found, (source, name)
            # The two engine platform branches currently declare identical interfaces.
            assert all(item == found[0] for item in found), (source, name, 'branch drift')
            lines.append(found[0])
            receipt.append({'context': namespace, 'source': str(path), 'class': name,
                            'branch_copies': len(found),
                            'source_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                            'declaration_sha256': hashlib.sha256(found[0].encode()).hexdigest()})
        lines.append('}')
    (options.output / 'actual_declarations.h').write_text('\n'.join(lines) + '\n')
    (options.output / 'declaration-receipt.json').write_text(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
