"""Keep complete provider socket-ownership methods; control only the socket/OS service type."""
from pathlib import Path
import hashlib
import sys

source = Path(sys.argv[1]).read_text()
out = ['#include "socket_fixture.hpp"\n']
for signature in ('void Network::ShutDown()', 'INetSocket* Network::CreateSocket(',
                  'bool Network::RemoveSocket(', 'NetSocket* Network::GetSocket('):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    original = source[start:end]
    body = original.replace('Network::', 'ControlledNetwork::').replace('NetSocket*', 'ControlledSocket*')
    body = body.replace('new NetSocket()', 'new ControlledSocket()').replace('IControlledSocket*', 'INetSocket*')
    out.append('// Original region SHA256: ' + hashlib.sha256(original.encode()).hexdigest() + '\n' + body)
Path(sys.argv[2]).write_text('\n\n'.join(out))
