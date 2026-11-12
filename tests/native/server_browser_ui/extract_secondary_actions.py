"""Extract actual browser actions for controlled CPU ownership/navigation coverage.

The UI shell is substituted by secondary_action_tests.cpp; method bodies remain exact source bytes.
Actual VGUI input/resource delivery is a separate Root-owned covering execution.
"""
import argparse
import hashlib
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
source = args.repo / 'GoldSrc-SDK/GameUI/native/ServerBrowserDialog.cpp'
text = source.read_text()
signatures = [
    'void ServerBrowserDialog::SelectVisible(',
    'void ServerBrowserDialog::SelectVisibleItems(',
    'void ServerBrowserDialog::MoveSelection(',
    'void ServerBrowserDialog::ScrollViewport(',
    'void ServerBrowserDialog::ReconcileSelection()',
    'void ServerBrowserDialog::ConnectSelection(',
    'BrowserDetailsDialog *ServerBrowserDialog::OpenDetails()',
]
blocks = []
for signature in signatures:
    start = text.index(signature)
    end = text.index('\n}', start) + len('\n}')
    blocks.append(text[start:end])
args.output.mkdir(parents=True, exist_ok=True)
(args.output / 'secondary_actions.inc').write_text('\n\n'.join(blocks) + '\n')
(args.output / 'secondary-extraction.json').write_text(json.dumps({
    'source': str(source.resolve()),
    'sourceSha256': hashlib.sha256(source.read_bytes()).hexdigest(),
    'signatures': signatures,
    'extractedSha256': hashlib.sha256((args.output / 'secondary_actions.inc').read_bytes()).hexdigest(),
}, indent=2) + '\n')
