"""Extract exact production reader bodies for the private Windows GUI control."""
import argparse
import hashlib
import json
from pathlib import Path

METHODS = (
    ('src/common/TextConsoleWin32.cpp', 'char* CTextConsoleWin32::GetLine()'),
    ('src/common/textconsole.cpp', 'bool CTextConsole::Init( IBaseSystem* system )'),
    ('src/common/textconsole.cpp', 'int CTextConsole::ReceiveNewline()'),
    ('src/common/textconsole.cpp', 'void CTextConsole::ReceiveStandardChar( char character )'),
)


def extract(repository, destination, receipt):
    bodies = []
    facts = []
    for relative, signature in METHODS:
        data = (repository / relative).read_bytes()
        text = data.decode('utf-8')
        start = text.index(signature)
        position = text.index('{', start) + 1
        depth = 1
        while depth:
            if text[position] == '{':
                depth += 1
            elif text[position] == '}':
                depth -= 1
            position += 1
        body = text[start:position]
        bodies.append(body)
        facts.append({'source': relative, 'source_sha256': hashlib.sha256(data).hexdigest(),
                      'method': signature, 'extracted_sha256': hashlib.sha256(body.encode()).hexdigest()})
    destination.write_text('\n\n'.join(bodies) + '\n')
    receipt.write_text(json.dumps(facts, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repository', type=Path, required=True)
    parser.add_argument('--destination', type=Path, required=True)
    parser.add_argument('--receipt', type=Path, required=True)
    args = parser.parse_args()
    extract(args.repository, args.destination, args.receipt)
