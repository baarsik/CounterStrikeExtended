"""Compile intact trusted production definitions; no replacement network/auth algorithm."""
import argparse
from pathlib import Path
import re


def extract(path, name):
    source = path.read_text(encoding='cp1252')
    pattern = r'^(?:static )?[\w:* \t]+?\b' + re.escape(name) + r'\([^;]*?\)\s*\{'
    matches = list(re.finditer(pattern, source, re.M))
    if len(matches) != 1:
        raise ValueError(f'{path}: expected one definition of {name}, found {len(matches)}')
    tokens = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', re.S)
    depth = 0
    for token in tokens.finditer(source, matches[0].end() - 1):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if depth == 0:
                return source[matches[0].start():token.end()]
    raise ValueError(f'{path}: unterminated {name}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('engine', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    groups = {
        'actual_connection.h': ('cl_main.cpp', ['CL_GetCDKeyHash', 'CL_SendConnectPacket']),
        'actual_steam_connection.h': ('sv_steam3.cpp', [
            'NET_NetAdr_GetIPNetworkByteOrder', 'NET_NetAdr_SetIP', 'NET_NetAdr_SetPort',
            'CSteam3Client::InitiateGameConnection', 'CSteam3Client::TerminateConnection']),
        'actual_md5.h': ('crc.cpp', ['MD5Init', 'MD5Update', 'MD5Final', 'MD5_F1', 'MD5_F3',
                                   'MD5_F4', 'MD5_Step', 'MD5Transform', 'MD5_Print']),
    }
    for output, (source, names) in groups.items():
        (args.output / output).write_text('\n\n'.join(extract(args.engine / source, n) for n in names) + '\n')
    info = (args.engine / 'info.cpp').read_text(encoding='cp1252')
    # Retain the whole implementation and its copyright, replacing only include dependencies.
    info = re.sub(r'^#include "[^"]+"\n', '', info, flags=re.M)
    (args.output / 'actual_info.h').write_text(info)
