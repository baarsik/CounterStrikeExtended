#!/usr/bin/env python3
"""Read-only source inventory; candidate registrations are not represented as binary export qualification."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[3]
SDK = ROOT / 'src'
PUBLISHERS = {'cs_server': 'common/game_module_entry_impl.h', 'hl_server': 'common/game_module_entry_impl.h',
              'cs_client': 'common/client_module_entry_impl.h', 'hl_client': 'common/client_module_entry_impl.h',
              'filesystem_stdio': 'filesystem/FileSystem_Stdio.cpp'}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--mac-database', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    options.output.mkdir(parents=True, exist_ok=True)
    sys.path.insert(0, str(ROOT / 'tools'))
    from cmake_inventory import query_targets
    records = query_targets(ROOT, configuration='Release', target_os='windows', architecture='x86')
    database = json.loads(options.mac_database.read_text())
    owners = {record['target'] for record in records}
    assert len(owners) == 19
    report = []
    evidence = {}
    overlays = {}
    for record in records:
        target = record['target']
        windows = [item['path'] for item in record['sources'] if item['language'] in ('C', 'CXX')]
        mac = [entry for entry in database if re.search(r'(^|/)targets/' + re.escape(target) + '/', entry['output'])]
        sources = set(windows)
        sources.update(entry['file'] for entry in mac)
        if overlay := record.get('source_overlay'):
            overlays[target] = overlay
            original_root = Path(overlay['root'])
            # Source snippets below describe checked-in CSE code. The original
            # submodule text does not reproduce the patched compilation inputs.
            sources = {source for source in sources if not Path(source).is_relative_to(original_root)}
        factories = []
        loaders = []
        for source in sorted(sources):
            path = Path(source)
            if not path.is_file() or SDK not in path.parents:
                continue
            text = path.read_text(errors='replace')
            relevant = []
            for number, line in enumerate(text.splitlines(), 1):
                if line.lstrip().startswith('//'):
                    continue
                if re.search(r'\bEXPOSE_\w+\s*\(|\b(?:static\s+)?InterfaceReg\s+\w+\s*\(', line):
                    factories.append({'source': str(path.relative_to(ROOT)), 'line': number, 'text': line.strip()})
                    relevant.append(number)
                if re.search(r'\b(?:Sys_GetFactory|Sys_LoadModule|LoadLibrary[A-Z]?|GetProcAddress|CheckedFactory|CompatibleFactory)\s*\(', line):
                    loaders.append({'source': str(path.relative_to(ROOT)), 'line': number, 'text': line.strip()})
                    relevant.append(number)
            if relevant:
                evidence[str(path.relative_to(ROOT))] = {'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                                                         'lines': sorted(set(relevant))}
        links = sorted(set(record['link_libraries']) & owners)
        report.append({'target': target, 'runtime': record['runtime'],
                       'windows_retained_cpp_count': len(windows), 'mac_selected_tu_count': len(mac),
                       'descriptor_publisher': PUBLISHERS.get(target), 'direct_owned_links': links,
                       'factory_candidates': factories, 'loader_sites': loaders,
                       'coverage': 'CMake Windows source selections plus current Mac recipes; metadata query does not qualify a Windows compiler or provider'})
    cmake_inputs = sorted((ROOT / 'cmake').rglob('*.cmake')) + sorted((ROOT / 'cmake').rglob('CMakeLists.txt'))
    cmake_inputs += [ROOT / 'CMakeLists.txt', ROOT / 'tools/cmake_inventory.py']
    overlay_patches = [Path(overlay['patch']) for overlay in overlays.values()]
    payload = {'inputs': {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                          for path in cmake_inputs + overlay_patches},
               'source_overlays': overlays,
               'targets': sorted(report, key=lambda row: row['target']), 'source_evidence': evidence}
    payload['inputs'][str(options.mac_database)] = hashlib.sha256(options.mac_database.read_bytes()).hexdigest()
    (options.output / 'inventory.json').write_text(json.dumps(payload, indent=2))
    lines = ['| Target | Descriptor publisher | Factory candidates | Owned direct links |',
             '| --- | --- | ---: | --- |']
    for row in payload['targets']:
        lines.append('| ' + row['target'] + ' | ' + (row['descriptor_publisher'] or 'MISSING') + ' | '
                     + str(len(row['factory_candidates'])) + ' | ' + ', '.join(row['direct_owned_links']) + ' |')
    (options.output / 'inventory-summary.md').write_text('\n'.join(lines) + '\n')
    print('inventoried', len(report), 'production targets;', len(evidence), 'source evidence files')


if __name__ == '__main__':
    main()
