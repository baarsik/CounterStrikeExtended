"""Import selected voice methods verbatim; never execute a voice or clipboard provider."""
import argparse
import hashlib
import json
from pathlib import Path
import re

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--voice-source', type=Path)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
records = {}


def extract(path, expressions):
    data = path.read_bytes()
    text = data.decode('cp1252')
    bodies = []
    for expression in expressions:
        matches = list(re.finditer(expression, text, re.MULTILINE))
        assert len(matches) == 1, (path, expression, len(matches))
        begin = matches[0].start()
        opening = text.index('{', matches[0].end())
        # These retained methods are global definitions with a column-zero
        # closing brace. Raw brace counting cannot cross alternative #if arms:
        # each arm may open the same scope that a shared closing brace ends.
        closing = re.search(r'^}', text[opening:], re.MULTILINE)
        assert closing is not None, (path, expression, 'Missing global method boundary')
        end = opening + closing.end()
        bodies.append(text[begin:end])
    records[str(path)] = {
        'source_sha256': hashlib.sha256(data).hexdigest(),
        'method_sha256': [hashlib.sha256(body.encode('cp1252')).hexdigest() for body in bodies],
    }
    return '\n\n'.join(bodies) + '\n'


voice = extract(args.voice_source or args.repo / 'GoldSrc-SDK/cl_dll/voice_status.cpp', [
    r'^void CVoiceStatus::UpdateServerState\(bool bForce\)',
    r'^void CVoiceStatus::HandleVoiceMaskMsg\(int iSize, void \*pbuf\)',
])
ban = extract(args.repo / 'GoldSrc-SDK/game_shared/voice_banmgr.cpp', [
    r'^unsigned char HashPlayerID\(char const playerID\[16\]\)',
    r'^CVoiceBanMgr::CVoiceBanMgr\(\)',
    r'^CVoiceBanMgr::~CVoiceBanMgr\(\)',
    r'^void CVoiceBanMgr::Term\(\)',
    r'^bool CVoiceBanMgr::GetPlayerBan\(char const playerID\[16\]\)',
    r'^void CVoiceBanMgr::SetPlayerBan\(char const playerID\[16\], bool bSquelch\)',
    r'^void CVoiceBanMgr::Clear\(\)',
    r'^CVoiceBanMgr::BannedPlayer\* CVoiceBanMgr::InternalFindPlayerSquelch\(char const playerID\[16\]\)',
    r'^CVoiceBanMgr::BannedPlayer\* CVoiceBanMgr::AddBannedPlayer\(char const playerID\[16\]\)',
])
(args.output / 'actual_voice_methods.inc').write_bytes(voice.encode('cp1252'))
(args.output / 'actual_ban_methods.inc').write_bytes(ban.encode('cp1252'))
(args.output / 'imports.json').write_text(json.dumps(records, indent=2) + '\n')
