"""Compile complete production network bodies with controlled engine dependencies.

This fixture deliberately excludes netchan, Steam and the rest of the engine;
the UDP receive, queue ownership and worker lifecycle functions remain intact.
"""
import argparse
from pathlib import Path
import re


def extract(text, name):
    matches = list(re.finditer(
        r'^(?:static )?(?:void|bool|int|qboolean|net_messages_t\*) ' + re.escape(name)
        + r'\([^;]*?\)\s*\{', text, re.M))
    if len(matches) != 1:
        raise ValueError(f'Expected exactly one definition of {name}')
    start = matches[0].start()
    depth = 0
    tokens = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', re.S)
    for token in tokens.finditer(text, matches[0].end() - 1):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if depth == 0:
                return text[start:token.end()]
    raise ValueError(f'Unterminated function {name}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    source = args.source.read_text(encoding='cp1252')
    names = ['SockadrToNetadr', 'NET_QueuePacket', 'NET_GetPacket', 'NET_StartThread',
             'NET_StopThread', 'NET_Config', 'NET_Shutdown', 'NET_AllocMsg', 'NET_FreeMsg',
             'NET_AllocateQueues', 'NET_FlushQueues']
    worker_names = ['NET_WorkerSockets', 'NET_WorkerQueuePacket']
    worker_names = [name for name in worker_names if re.search(r'^static .*\b' + name + r'\(', source, re.M)]
    names[3:3] = worker_names
    helpers = ['NET_PosixSocketCount', 'NET_PosixAddressLength', 'NET_PosixReceiveFrom']
    prelude = '#if !defined(_WIN32)\n' + '\n\n'.join(extract(source, name) for name in helpers) + '\n#endif\n'
    args.output.write_text(prelude + '\n\n'.join(extract(source, name) for name in names) + '\n', encoding='utf-8')
    structures = []
    for tag, name in [('packetlag_s', 'packetlag_t'), ('net_messages_s', 'net_messages_t')]:
        matches = re.findall(r'typedef struct ' + tag + r'\s*\{[^}]*\}\s*' + name + r';', source)
        if len(matches) != 1:
            raise ValueError(f'Expected exactly one complete definition of {name}')
        structures += matches
    args.output.with_name('actual_network_types.h').write_text('\n\n'.join(structures) + '\n', encoding='utf-8')
