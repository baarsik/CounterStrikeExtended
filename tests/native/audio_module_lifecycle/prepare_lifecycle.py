"""Compile exact owned shutdown, native DMA close and Run bodies in the unload fixture."""
from pathlib import Path
import hashlib
import json
import re
import sys

root = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
receipts = []


def extract(name, expression, occurrence=0):
    path = root / name
    text = path.read_bytes().decode('latin1')
    match = list(re.finditer(expression, text))[occurrence]
    begin = match.start()
    end = text.index('{', match.end()) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    body = text[begin:end]
    receipts.append({'source': name, 'source_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                     'body_sha256': hashlib.sha256(body.encode('latin1')).hexdigest()})
    return f'#line {text.count(chr(10), 0, begin) + 1} "{path.as_posix()}"\n' + body + '\n'


boundary = extract('src/engine/cd.cpp', r'bool CDAudio_ShutdownForModuleUnload\(')
boundary += extract('src/engine/cd.cpp', r'cse::AudioModuleShutdown::~AudioModuleShutdown\(')
dma = extract('src/engine/snd_sdl.cpp', r'extern "C" void SNDDMA_Shutdown\(')
run = extract('src/engine/sys_dll2.cpp', r'int CEngineAPI::Run\(', 1)
(out / 'production_audio_boundary.inc').write_text(boundary)
(out / 'production_dma_close.inc').write_text(dma)
(out / 'production_run.inc').write_text(run)
(out / 'production_bodies.json').write_text(json.dumps(receipts, indent=2) + '\n')
