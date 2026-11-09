"""Read only approved Steam resource text; preserve input hashes without account metadata."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil

p = argparse.ArgumentParser()
p.add_argument('--game-root', type=Path, required=True)
p.add_argument('--metadata-root', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
assert not a.output.exists()
files = []
for family in ('cstrike', 'valve', 'platform'):
    directory = a.game_root / family / 'resource'
    if directory.is_dir():
        files += [x for x in directory.rglob('*') if x.is_file() and x.suffix.lower() in ('.txt', '.res')]
assert len(files) <= 2000 and sum(x.stat().st_size for x in files) <= 32 * 1024 * 1024
records = []
for source in sorted(files):
    before = hashlib.sha256(source.read_bytes()).hexdigest()
    relative = source.relative_to(a.game_root)
    target = a.output / 'resource-text' / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)
    after = hashlib.sha256(source.read_bytes()).hexdigest()
    copied = hashlib.sha256(target.read_bytes()).hexdigest()
    assert before == after == copied, source
    records.append({'source': str(source), 'relative': str(relative), 'bytes': target.stat().st_size,
                    'before_sha256': before, 'after_sha256': after, 'copied_sha256': copied})
metadata = []
for app in ('10', '70'):
    source = a.metadata_root / ('appmanifest_' + app + '.acf')
    if source.is_file():
        raw = source.read_bytes()
        text = raw.decode('utf-8-sig')
        selected = {}
        for name in ('appid', 'name', 'installdir', 'buildid', 'LastUpdated', 'StateFlags'):
            found = re.search(r'"' + name + r'"\s*"([^"\n]*)"', text)
            if found:
                selected[name] = found.group(1)
        depot = re.search(r'"InstalledDepots"\s*\{(.*?)\n\s*\}', text, re.S)
        # Only public depot/manifest identities are retained, never LastOwner/UserConfig.
        selected['depot_manifests'] = re.findall(r'"manifest"\s*"([0-9]+)"', text)
        after = hashlib.sha256(source.read_bytes()).hexdigest()
        before = hashlib.sha256(raw).hexdigest()
        assert before == after
        metadata.append({'source': str(source), 'before_sha256': before, 'after_sha256': after,
                         'selected_build_identity': selected})
a.output.mkdir(exist_ok=True)
(a.output / 'inputs.json').write_text(json.dumps({'game_root': str(a.game_root), 'files': records,
    'Steam_metadata': metadata, 'scope': 'newly observed resource corpus, not all dynamic/custom/chat formats'}, indent=2) + '\n')
print('copied', len(records), 'resource text files;', sum(x['bytes'] for x in records), 'bytes; all source hashes stable')
