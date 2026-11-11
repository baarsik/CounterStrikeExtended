"""Extract complete current owners with unique anchors and a source hash receipt."""
import argparse
import hashlib
import json
import re
from pathlib import Path


def between(source, start, end, definition=True):
    endings = list(re.finditer(re.escape(end) + (r'[^\n]*\n\{' if definition else ''), source))
    if source.count(start) != 1 or len(endings) != 1:
        raise ValueError(f"Missing or nonunique owner anchor: {start!r}, {end!r}")
    begin = source.index(start)
    finish = endings[0].start()
    if finish <= begin:
        raise ValueError(f"Reversed owner anchors: {start!r}")
    return source[begin:finish]


def extract(root, output):
    receipts = []
    bodies = []
    selections = {
        'host_cmd.cpp': [
            ('void Host_Reload_f()\n{', '\nvoid Host_Motd_f()'),
            ('int __cdecl Host_Load( const char* pName )\n{', '\nvoid FileCopy('),
        ],
        'cmd.cpp': [
            ('static void Cbuf_AddTextToBuffer(', '\nvoid Cbuf_AddTextToUntrustedBuffer('),
            ('void Cbuf_AddText( const char* text )\n{', '\nvoid Cbuf_AddFilteredText('),
            ('void Cbuf_ExecuteCommandsFromBuffer(', '\nint Cmd_Argc()'),
            ('void Cbuf_Execute()\n{', '\nvoid Cmd_Wait_f()'),
        ],
        'cl_main.cpp': [
            ('void CL_Disconnect()\n{', '\nvoid CL_ClearClientState()'),
            ('void CL_ClearState( qboolean clearMemory )\n{', '\nvoid AddStartupTiming('),
            ('void CL_Disconnect_f()\n{', '\nint CL_GetFragmentSize('),
        ],
        'host.cpp': [
            ('qboolean Host_IsServerActive()\n{', '\nqboolean Host_IsSinglePlayerGame('),
            ('void Host_ClearMemory( qboolean quiet )\n{', '\nvoid SV_BroadcastPrintf('),
        ],
        'pr_edict.cpp': [('edict_t* ED_Alloc()\n{', '\nvoid SaveSpawnParms(')],
    }
    for name, anchors in selections.items():
        path = root / 'GoldSrc-SDK/engine' / name
        raw = path.read_bytes()
        source = raw.decode().replace('\r\n', '\n')
        if name == 'host_cmd.cpp' and 'enum class HostLoadFailurePolicy' in source:
            declaration_anchors = ('namespace {\nenum class HostLoadFailurePolicy', '\nint Host_ValidSave();')
            declaration = between(source, *declaration_anchors, definition=False)
            bodies.insert(0, declaration)
            anchors[1] = ('namespace {\nint Host_LoadWithFailurePolicy(', '\nvoid FileCopy(')
            receipts.append({'path': str(path), 'declaration_anchors': declaration_anchors})
        for start, end in anchors:
            bodies.append(between(source, start, end))
        receipts.append({'path': str(path), 'sha256': hashlib.sha256(raw).hexdigest(), 'anchors': anchors})
    output.write_text('\n'.join(bodies))
    output.with_suffix('.json').write_text(json.dumps(receipts, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('root', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    extract(args.root, args.output)
