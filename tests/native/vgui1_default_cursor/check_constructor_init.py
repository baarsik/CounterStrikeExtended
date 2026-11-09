"""Defined compiler-AST diagnostic; never execute the pre-fix dc_none path."""
import argparse
import json
import sys
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('ast', type=Path)
args = parser.parse_args()


def walk(node):
    yield node
    for child in node.get('inner', []):
        yield from walk(child)


tree = json.loads(args.ast.read_text())
constructors = [node for node in walk(tree)
                if node.get('kind') == 'CXXConstructorDecl' and node.get('name') == 'CursorBitmap'
                and node.get('type', {}).get('qualType') == 'void (vgui::Cursor::DefaultCursor)'
                and any(child.get('kind') == 'CompoundStmt' for child in node.get('inner', []))]
assert len(constructors) == 1, 'unknown actual constructor AST'
constructor = constructors[0]
body = next(child for child in constructor['inner'] if child['kind'] == 'CompoundStmt')
first = body['inner'][0]
assert first['kind'] == 'IfStmt', 'unknown early-return control flow'
assert any(node.get('referencedDecl', {}).get('name') == 'dc_none' for node in walk(first))
assert any(node['kind'] == 'ReturnStmt' for node in walk(first))
initializers = [child for child in constructor['inner'] if child['kind'] == 'CXXCtorInitializer'
                and child.get('anyInit', {}).get('name') == '_hotspot']
if not initializers:
    print('ERROR: actual CursorBitmap(dc_none) returns before either int _hotspot[2] cell is initialized.')
    print('This is a compiler-AST constructor-init diagnostic; no object or indeterminate value was read.')
    sys.exit(1)
assert len(initializers) == 1
values = [node['value'] for node in walk(initializers[0]) if node['kind'] == 'IntegerLiteral']
assert values == ['0', '0'], ('unexpected hotspot initial values', values)
print('PASS: actual CursorBitmap initializes both int _hotspot[2] cells to zero before the dc_none return.')
