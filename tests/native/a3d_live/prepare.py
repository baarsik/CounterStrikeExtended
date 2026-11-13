"""Select exact native DMA bodies and verify fixture records with the owning recipe."""
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

root = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
database = Path(sys.argv[3]).resolve()
source = root / 'src/engine/snd_sdl.cpp'
text = source.read_text()
sound = root / 'src/engine/sound.h'
record = re.search(r'typedef struct\s*\{[^{}]*\}\s*dma_t;', sound.read_text()).group()
boolean = re.search(r'typedef int qboolean;', (root / 'src/engine/const.h').read_text()).group()
names = ['SNDDMA_Shutdown', 'SNDDMA_Init', 'SNDDMA_LockBuffer', 'SNDDMA_UnlockBuffer',
         'SNDDMA_GetDMAPos', 'S_BlockSound', 'S_UnblockSound']
records = []
bodies = []
for name in names:
    match = re.search(r'extern "C" [^\n]+\b' + name + r'\([^\n]*\)\s*\{', text)
    end = match.end()
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    body = text[match.start():end]
    records.append({'name': name, 'sha256': hashlib.sha256(body.encode()).hexdigest()})
    bodies.append(f'#line {text[:match.start()].count(chr(10)) + 1} "{source}"\n{body}\n')
(out / 'production_dma.inc').write_text('\n'.join(bodies))
(out / 'production_dma_record.h').write_text(boolean + '\n' + record + '\n')
fields = re.findall(r'\b(\w+)\s*;', re.sub(r'//[^\n]*', '', record))[:-1]
check = '#include <cstddef>\n#include <type_traits>\n#include "' + str(source) + '"\n'
check += 'namespace fixture {\n' + boolean + '\n' + record + '\n}\n'
check += 'static_assert(sizeof(dma_t) == sizeof(fixture::dma_t));\n'
check += 'static_assert(alignof(dma_t) == alignof(fixture::dma_t));\n'
for field in fields:
    check += f'static_assert(offsetof(dma_t, {field}) == offsetof(fixture::dma_t, {field}));\n'
    check += f'static_assert(std::is_same_v<decltype(dma_t::{field}), decltype(fixture::dma_t::{field})>);\n'
for name in ['SNDDMA_Shutdown', 'SNDDMA_UnlockBuffer', 'S_BlockSound', 'S_UnblockSound']:
    check += f'static_assert(std::is_same_v<decltype(&{name}), void(*)()>);\n'
check += 'static_assert(std::is_same_v<decltype(&SNDDMA_Init), qboolean(*)()>);\n'
check += 'static_assert(std::is_same_v<decltype(&SNDDMA_LockBuffer), bool(*)()>);\n'
check += 'static_assert(std::is_same_v<decltype(&SNDDMA_GetDMAPos), int(*)()>);\n'
checkpath = out / 'dma_contract.cpp'
checkpath.write_text(check)
entries = [entry for entry in json.loads(database.read_text())
           if entry['file'] == str(source) and 'targets/hw_engine/' in entry['command']]
assert len(entries) == 1
entry = entries[0]
argv = shlex.split(entry['command'])
index = argv.index('-o')
del argv[index:index + 2]
argv.remove('-c')
argv[argv.index(str(source))] = str(checkpath)
argv += ['-fsyntax-only']
process = subprocess.run(argv, cwd=entry['directory'], capture_output=True, text=True)
(out / 'dma_contract.log').write_text(process.stdout + process.stderr)
(out / 'production-extraction.json').write_text(json.dumps({
    'source': str(source), 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
    'record_source': str(sound), 'record_source_sha256': hashlib.sha256(sound.read_bytes()).hexdigest(),
    'record_fields': fields, 'bodies': records, 'contract_argv': argv, 'contract_exit': process.returncode,
}, indent=2))
sys.exit(process.returncode)
