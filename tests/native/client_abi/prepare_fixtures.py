"""Compile exact client entry bodies and mutate real declarations for ABI fault fixtures."""
from pathlib import Path
import hashlib
import json
import re
import sys

root = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
sys.path.insert(0, str(root / 'tools'))
from client_module_layouts import fields, clean

out.mkdir(parents=True, exist_ok=True)
receipts = []


def body(text, expression):
    match = re.search(expression, text)
    assert match, expression
    start = match.start()
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return start, text[start:end]


members = fields((root / 'GoldSrc-SDK/engine/APIProxy.h').read_bytes().decode('latin1'), 'cldll_func_t')
for variant, prefix in [('hl', 'cl_dll'), ('cs', 'cstrike/cl_dll')]:
    blocks = []
    path = root / 'GoldSrc-SDK' / prefix / 'cdll_int.cpp'
    text = path.read_bytes().decode('latin1')
    for expression in (r'(?:extern "C" )?int (?:CL_DLLEXPORT|EXPORT) Initialize\(',
                       r'extern "C" void (?:CL_DLLEXPORT|EXPORT) F\('):
        start, code = body(text, expression)
        blocks.append(f'#line {text.count(chr(10), 0, start) + 1} "{path.as_posix()}"\n{code}\n')
        receipts.append({'path': str(path), 'source_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                         'body_sha256': hashlib.sha256(code.encode('latin1')).hexdigest()})
    initializers = re.search(r'cldll_func_t\s+\w+\s*=\s*\{(.*?)\};', blocks[-1], re.S)[1].split(',')
    stubs = []
    for member, value in zip(members, initializers):
        name = re.sub(r'\([^)]*\)', '', value).strip()
        if not name or name in ('Initialize', 'HUD_GetStudioModelInterface', 'nullptr'):
            continue
        assert re.fullmatch(r'\w+', name), name
        alias = re.search(r'\b(\w+)\s+' + member + r'\s*;', clean((root / 'GoldSrc-SDK/engine/APIProxy.h').read_bytes().decode('latin1')))[1]
        declaration = re.search(r'\btypedef\s+([^;]+?)\(\s*\*\s*' + alias + r'\s*\)\s*\(([^;]*)\)\s*;',
                                clean((root / 'GoldSrc-SDK/engine/APIProxy.h').read_bytes().decode('latin1')), re.S)
        assert declaration, alias
        result = declaration[1].strip()
        arguments = declaration[2].strip()
        returned = 'return;' if result == 'void' else 'return {};'
        stubs.append(f'extern "C" FIXTURE_EXPORT {result} {name}({arguments}) {{ {returned} }}')
    path = root / 'GoldSrc-SDK' / prefix / 'GameStudioModelRenderer.cpp'
    text = path.read_bytes().decode('latin1')
    start, code = body(text, r'int CL_DLLEXPORT HUD_GetStudioModelInterface\(')
    blocks.append(f'#line {text.count(chr(10), 0, start) + 1} "{path.as_posix()}"\n{code}\n')
    receipts.append({'path': str(path), 'source_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                     'body_sha256': hashlib.sha256(code.encode('latin1')).hexdigest()})
    (out / (variant + '_entries.inc')).write_text('\n'.join(blocks))
    pragmas = ['#ifdef _MSC_VER', '#pragma warning(push)', '#pragma warning(disable: 4100)', '#else',
               '#pragma clang diagnostic push', '#pragma clang diagnostic ignored "-Wunused-parameter"', '#endif']
    endings = ['#ifdef _MSC_VER', '#pragma warning(pop)', '#else', '#pragma clang diagnostic pop', '#endif']
    (out / (variant + '_stubs.inc')).write_text('\n'.join(pragmas + stubs + endings) + '\n')

api = (root / 'GoldSrc-SDK/engine/APIProxy.h').read_bytes().decode('latin1')
for name, before, after in [
    ('prototype', r'typedef float(\s*\(\*pfnEngSrc_GetClientMaxspeed_t\s*\))', r'typedef int\1'),
    ('destination', r'(\(\*DST_INITIALIZE_FUNC\)\s*\(\s*struct cl_enginefuncs_s\*\*,\s*)int\s*\*', r'\1float *'),
    ('mouse_source', r'typedef int(\s*\(\*pfnEngSrc_pfnGetClientMousePositionAndCenter_t\))', r'typedef float\1'),
    ('mouse_destination', r'(\(\*pfnEngDst_pfnGetClientMousePositionAndCenter_t\)\(int \*\*, int \*\*, )int', r'\1float')]:
    changed, count = re.subn(before, after, api)
    assert count == 1, (name, count)
    (out / ('mutant_' + name + '_APIProxy.h')).write_bytes(changed.encode('latin1'))
event = (root / 'GoldSrc-SDK/common/event_api.h').read_bytes().decode('latin1')
changed, count = re.subn(r'int(\s*\(\s*\*EV_IsLocal\s*\))', r'float\1', event)
assert count == 1
(out / 'mutant_event_api.h').write_bytes(changed.encode('latin1'))
model = (root / 'GoldSrc-SDK/common/com_model.h').read_bytes().decode('latin1')
model = model.replace('player_info_s', 'fixture_prefix_player_info_s').replace('player_info_t', 'fixture_prefix_player_info_t')
owner = (root / 'GoldSrc-SDK/engine/client.h').read_bytes().decode('latin1')
start = owner.index('typedef struct player_info_s')
end = owner.index('} player_info_t;', start) + len('} player_info_t;')
constant = re.search(r'#define HASHED_CDKEY_BYTES\s+[^\n]+', owner).group()
constant += '\n' + re.search(r'#define MAX_PLAYERNAME\s+[^\n]+', owner).group()
(out / 'engine_model_view.h').write_bytes((model + '\n' + constant + '\n' + owner[start:end] + '\n').encode('latin1'))
(out / 'extracted-entries.json').write_text(json.dumps(receipts, indent=2) + '\n')
from client_module_layouts import FAMILIES
facts = ['struct FixtureLayoutFact { const char* name; unsigned long long value; };',
         'static const FixtureLayoutFact fixtureLayoutFacts[] = {']
for family in ('MovementRecords', 'ModelRecords'):
    for name, header in FAMILIES[family]:
        facts.append('{"' + name + '.sizeof", sizeof(' + name + ')},')
        facts.append('{"' + name + '.alignof", alignof(' + name + ')},')
        facts.append('{"' + name + '.contract", cse::abi::ClientLayout_' + name + '},')
        for field in fields((root / 'GoldSrc-SDK' / header).read_bytes().decode('latin1'), name):
            access = f'(({name}*)nullptr)->{field}'
            facts.append('{"' + name + '.' + field + '", cse::abi::LayoutHash({'
                         + f'offsetof({name}, {field}), sizeof({access}), alignof(decltype({access})), '
                         + f'cse::abi::ClientMemberType<decltype({access})>::Hash()' + '})},')
facts.append('};')
(out / 'movement_diagnostics.inc').write_text('\n'.join(facts) + '\n')

engine_path = root / 'GoldSrc-SDK/engine/cdll_int.cpp'
engine = engine_path.read_bytes().decode('latin1')
functions = []
for expression in (r'static void ClearOwnedClientBindings\(', r'static bool ValidateOwnedClientLibrary\(',
                   r'static bool ValidateOwnedClientFunctions\(', r'static void ReleaseOwnedClientLibrary\(',
                   r'int LoadSecureClient\(', r'template <typename ExportFunction>\s*static void ClientDLL_LoadRequiredExport\(',
                   r'void LoadInsecureClient\('):
    start, code = body(engine, expression)
    functions.append(f'#line {engine.count(chr(10), 0, start) + 1} "{engine_path.as_posix()}"\n{code}\n')
(out / 'engine_loader_bodies.inc').write_text('\n'.join(functions))
exports = (root / 'tests/native/client_abi/module.def').read_text()
extra = re.findall(r'Apply\(\w+, "([^"]+)"\)', (root / 'GoldSrc-SDK/common/client_module_required_exports.h').read_text())
existing = set(exports.split())
exports += ''.join('    ' + name + '\n' for name in extra if name not in existing)
(out / 'fixture_module.def').write_text(exports)
(out / 'fixture_module_without_descriptor.def').write_text(exports.replace('    CseGetModuleAbi\n', ''))

(out / 'fixture_module_without_initializer.def').write_text(exports.replace('    CseInitializeClientModule\n', ''))
