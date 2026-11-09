"""Keep exact production transport methods and helpers in a controlled native-API context."""
from pathlib import Path
import argparse
import re


def extract(text, name):
    matches = list(re.finditer(r'^(?:static )?(?:const )?[\w:*]+\s+' + re.escape(name) + r'\([^;]*?\)\s*\{', text, re.M))
    if len(matches) != 1:
        raise ValueError('Expected one production definition of ' + name)
    match = matches[0]
    depth = 0
    tokens = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\x27(?:\\.|[^\x27\\])*\x27|[{}]', re.S)
    for token in tokens.finditer(text, match.end() - 1):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if depth == 0:
                return text[match.start():token.end()]
    raise ValueError('Unterminated production function ' + name)


def run(args):
    text = args.source.read_text(encoding='cp1252')
    args.output.mkdir(parents=True, exist_ok=True)
    helpers = ['NET_PosixSocketCount', 'NET_PosixAddressLength', 'NET_PosixSendTo', 'NET_PosixReceiveFrom',
               'NET_PosixBuildReadSet', 'NET_PosixCloseOpenFailure']
    selected = [name for name in helpers if re.search(r'^\w+ ' + name + r'\(', text, re.M)]
    (args.output / 'actual_posix_helpers.h').write_text('#if !defined(_WIN32)\n' + '\n\n'.join(extract(text, name) for name in selected) + '\n#endif\n')
    for filename, names in [('actual_open.h', ['NET_ErrorString', 'NET_StringToSockaddr', 'NET_IPSocket']),
                            ('actual_transport.h', ['SockadrToNetadr', 'NET_AdrToString', 'NET_SendLong', 'NET_SendToImpl', 'NET_Sleep', 'NET_Sleep_Timeout'])]:
        bodies = []
        for name in names:
            body = extract(text, name)
            if name == 'NET_StringToSockaddr':
                prefix = '#if defined( _MSC_VER )\n#pragma warning( push )\n#pragma warning( disable : 4996 )\n#endif'
                suffix = '#if defined( _MSC_VER )\n#pragma warning( pop )\n#endif'
                assert prefix in text and suffix in text
                body = prefix + '\n' + body + '\n' + suffix
            bodies.append(body)
        (args.output / filename).write_text('\n\n'.join(bodies) + '\n')
    constants = []
    for name in ['NET_SOCKET_SOURCES', 'MAX_ROUTABLE_PACKET', 'SPLIT_PAYLOAD_BYTES', 'SPLIT_PACKET_ID',
                 'SPLIT_PACKET_NUMBER_SHIFT', 'SHOW_SPLIT_PACKETS', 'SOCKET_ADDRESS_TEXT_BYTES', 'PORT_ANY']:
        matches = re.findall(r'^constexpr [\w:<>]+ ' + name + r'\s*=[^;]*;', text, re.M)
        if len(matches) != 1:
            raise ValueError('Expected one production constant ' + name)
        constants += matches
    packet = re.search(r'#pragma pack\( push, 1 \).*?#pragma pack\( pop \)\s*static_assert\( sizeof\( SPLITPACKET \) == 9 \);', text, re.S)
    assert packet
    start = text.index('namespace\n{\n#if defined( CSE_ENGINE_WSOCK32 )')
    end = text.index('\n}\n#endif', start) + 2
    block = '#if defined(_WIN32)\n' + text[start:end] + '\n#endif\n'
    (args.output / 'actual_types.h').write_text(packet[0] + '\n' + '\n'.join(constants) + '\n' + block)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    run(parser.parse_args())
