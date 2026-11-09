#!/usr/bin/env python3
"""Read-only source inventory; candidate registrations are not represented as binary export qualification."""
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
SDK = ROOT / 'GoldSrc-SDK'
PUBLISHERS = {'cs_server': 'common/game_module_entry_impl.h', 'hl_server': 'common/game_module_entry_impl.h',
              'cs_client': 'common/client_module_entry_impl.h', 'hl_client': 'common/client_module_entry_impl.h',
              'filesystem_stdio': 'filesystem/FileSystem_Stdio.cpp'}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--mac-database', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    options.output.mkdir(parents=True, exist_ok=True)
    selections = ROOT / 'docs/msbuild/repository-selections.json'
    manifests = json.loads(selections.read_text())
    database = json.loads(options.mac_database.read_text())
    owners = {record['target'] for record in manifests}
    assert len(owners) == 19
    report = []
    evidence = {}
    for record in manifests:
        target = record['target']
        config = record['configurations']['Release']
        windows = [item['path'] for item in config['items'] if item['kind'] == 'ClCompile'
                   and item['metadata'].get('ExcludedFromBuild') != 'true']
        mac = [entry for entry in database if re.search(r'(^|/)targets/' + re.escape(target) + '/', entry['output'])]
        sources = set(path.replace('${CSE_SDK_DIR}', str(SDK)) for path in windows)
        sources.update(entry['file'] for entry in mac)
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
        cmake = ROOT / 'cmake/targets' / target / 'CMakeLists.txt'
        links = sorted(set(re.findall(r'target_link_libraries\(' + re.escape(target) + r' PRIVATE ([A-Za-z_]\w*)\)',
                                     cmake.read_text())) & owners)
        report.append({'target': target, 'project': record['project'], 'runtime': record.get('runtime'),
                       'windows_retained_cpp_count': len(windows), 'mac_selected_tu_count': len(mac),
                       'descriptor_publisher': PUBLISHERS.get(target), 'direct_owned_links': links,
                       'factory_candidates': factories, 'loader_sites': loaders,
                       'coverage': 'retained Windows selections plus current Mac recipes; conditional source candidates require platform reconciliation'})
    payload = {'inputs': {str(selections): hashlib.sha256(selections.read_bytes()).hexdigest(),
                          str(options.mac_database): hashlib.sha256(options.mac_database.read_bytes()).hexdigest()},
               'targets': sorted(report, key=lambda row: row['target']), 'source_evidence': evidence}
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
