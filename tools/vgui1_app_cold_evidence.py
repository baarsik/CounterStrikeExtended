#!/usr/bin/env python3
"""Compile-only actual App/Dar evidence and optional unselected constant metadata. Never loads images."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import sys

# Read-only reuse of the separately qualified bootstrap utility primitives.
import engine_bootstrap_evidence as common

FORMAT = 1
MAGIC = 0x4353454143303031
FAMILY = 0x4f4d0206
PREFIX_WORDS = 29
FIELD_WORDS = 12


def require(condition, message):
    common.require(condition, message)


def validate_schema(schema):
    require(schema['format'] == FORMAT and schema['runtime_version'] == 10
            and schema['descriptor_bytes'] == 288, 'Unknown AppCold schema/envelope')
    require(int(schema['family'], 0) == FAMILY, 'Unknown source-owned AppCold family')
    require(schema['method_count'] == 55 and len(schema['methods']) == 55, 'Incomplete App method schema')
    require(len(schema['fields']) == 31, 'Incomplete App/Dar field schema')
    for collection in ('methods', 'fields'):
        values = [row['id'] for row in schema[collection]]
        require(len(values) == len(set(values)), 'Duplicate semantic schema ID')
    require({row['record'] for row in schema['fields']} == set(schema['records']), 'Unknown/missing field record')
    require([row['id'] for row in schema['methods']] == list(range(1, 56)), 'Unexpected semantic method mapping')
    require(schema['static_fields'] == [{'id': 301, 'owner': 'vgui::App', 'name': '_instance',
            'type': 'vgui::App*', 'storage': 'provider-static-borrowed-engine-instance'}], 'Unknown static App field schema')
    require(schema['static_methods'] == [{'id': 401, 'owner': 'vgui::App', 'name': 'getInstance',
            'type': 'vgui::App* (*)()'}], 'Unknown imported static App method schema')
    require(schema['rtti_required'] is True, 'Actual engine App RTTI dependency must be explicit')
    require(schema['readiness'] == {'publisher_ready': False, 'cold_load_ready': False},
            'This foundation cannot claim publisher/cold readiness')


def method_type(row, variant=False):
    result = 'int' if variant else row['result']
    return result + ' (vgui::App::*)(' + row['arguments'] + ')'


def probe_source(context, schema, foundation):
    lines = ['#include "' + Path(context['source']).as_posix() + '"',
             '#include "' + foundation.as_posix() + '"',
             'namespace cse { namespace vgui1_app_cold_abi {',
             'static_assert(AppColdFamily == ' + str(FAMILY) + 'u, "Source-owned family mapping");']
    expressions = []
    for row in schema['fields']:
        tag = 'Field' + str(row['id'])
        lines.append('CSE_VGUI1_COLD_MEMBER(' + tag + ', ' + row['owner'] + ', ' + row['name'] + ');')
        lines.append('using ' + tag + 'Value = typename MemberValue<' + tag + 'Type>::Type;')
        value = tag + 'Value'
        if row['type'] == 'timing-signed32':
            shape = 'TimingFieldShape<' + value + '>::Value'
        else:
            lines.append('static_assert(std::is_same<' + value + ', ' + row['type']
                         + '>::value, "Actual field type drift: ' + row['name'] + '");')
            shape = 'TypeShape<' + value + '>::Value'
        expressions += [str(row['id']), shape, 'TypeShape<' + value + '>::Value',
                        'sizeof(' + value + ')', 'alignof(' + value + ')',
                        'std::rank<' + value + '>::value', 'std::extent<' + value + ', 0>::value',
                        'std::extent<' + value + ', 1>::value',
                        'std::is_signed<typename std::remove_all_extents<' + value + '>::type>::value',
                        'std::is_const<' + value + '>::value', 'std::is_volatile<' + value + '>::value',
                        str(list(schema['records']).index(row['record']) + 1)]
    for row in schema['static_fields']:
        tag = 'StaticField' + str(row['id'])
        lines.append('CSE_VGUI1_COLD_MEMBER(' + tag + ', ' + row['owner'] + ', ' + row['name'] + ');')
        value = 'typename std::remove_pointer<' + tag + 'Type>::type'
        lines.append('static_assert(std::is_same<' + value + ', ' + row['type']
                     + '>::value, "Actual static field type drift");')
        expressions += [str(row['id']), 'TypeShape<' + value + '>::Value',
                        'sizeof(' + value + ')', 'alignof(' + value + ')']
    for row in schema['static_methods']:
        tag = 'StaticMethod' + str(row['id'])
        lines.append('CSE_VGUI1_COLD_MEMBER(' + tag + ', ' + row['owner'] + ', ' + row['name'] + ');')
        lines.append('static_assert(std::is_same<' + tag + 'Type, ' + row['type']
                     + '>::value, "Actual static imported method drift");')
        expressions += [str(row['id']), 'FreeCallShape<' + tag + 'Type>::Value']
    for row in schema['methods']:
        tag = 'Method' + str(row['id'])
        if row['name'] == 'setMouseArena':
            lines += ['struct ' + tag + ' {', '    friend auto CseVgui1ColdMember(' + tag + ');', '};',
                      'template struct MemberAccess<' + tag + ', ' + row['type'] + ', static_cast<'
                      + row['type'] + '>(&vgui::App::' + row['name'] + ')>;',
                      'using ' + tag + 'Type = decltype(CseVgui1ColdMember(' + tag + '{}));']
        else:
            lines.append('CSE_VGUI1_COLD_MEMBER(' + tag + ', vgui::App, ' + row['name'] + ');')
        check = 'std::is_same<' + tag + 'Type, ' + row['type'] + '>::value'
        if row['native_provider_int_variant']:
            check += ' || std::is_same<' + tag + 'Type, ' + method_type(row, True) + '>::value'
        lines.append('static_assert(' + check + ', "Actual full method type drift: ' + row['name'] + '");')
        expressions += [str(row['id']), 'MethodShape<' + tag + 'Type>::Value']
    for enum_id, row in enumerate(schema['enums'], 1):
        enum_type = 'vgui::Cursor::DefaultCursor' if row['role'] == 'DefaultCursor' else 'vgui::' + row['role']
        expressions += [str(enum_id), 'sizeof(' + enum_type + ')', 'alignof(' + enum_type + ')',
                        'std::is_signed<typename std::underlying_type<' + enum_type + '>::type>::value',
                        'TypeShape<typename std::underlying_type<' + enum_type + '>::type>::Value',
                        str(len(row['values']))]
        for entry in row['values']:
            lines.append('static_assert(' + entry['expression'] + ' == ' + str(entry['expected'])
                         + ', "Actual enumerator drift");')
            expressions.append('static_cast<std::uint64_t>(' + entry['expression'] + ')')
    lines += ['#if defined(_WIN32)', 'constexpr std::uint64_t EvidencePlatform = 1;',
              '#elif defined(__APPLE__)', 'constexpr std::uint64_t EvidencePlatform = 2;',
              '#else', '#error AppCold compiler evidence supports only measured Windows/Apple contexts', '#endif', '#if defined(_CPPRTTI) || defined(__GXX_RTTI)',
              'constexpr std::uint64_t EvidenceRtti = 1;', '#else',
              'constexpr std::uint64_t EvidenceRtti = 0;', '#endif']
    prefix = [str(MAGIC) + 'ull', str(FORMAT), 'ContractRevision', 'OwnershipSignature()',
              'abi::RuntimeVersion', 'sizeof(abi::Descriptor)', 'abi::NativeArchitecture',
              'abi::NativeCompilerAbi', 'EvidencePlatform', 'sizeof(void*)', 'alignof(void*)',
              'sizeof(vgui::App)', 'alignof(vgui::App)', 'sizeof(vgui::Dar<vgui::SurfaceBase*>)',
              'alignof(vgui::Dar<vgui::SurfaceBase*>)', 'sizeof(vgui::Dar<vgui::TickSignal*>)',
              'alignof(vgui::Dar<vgui::TickSignal*>)', 'std::is_abstract<vgui::App>::value',
              'std::has_virtual_destructor<vgui::App>::value', 'std::is_trivially_destructible<vgui::App>::value',
              'std::is_polymorphic<vgui::App>::value', 'sizeof(void (vgui::App::*)())',
              'alignof(void (vgui::App::*)())', str(len(schema['fields'])), str(len(schema['methods'])),
              str(sum(len(row['values']) for row in schema['enums'])), 'EvidenceRtti',
              str(len(schema['static_fields'])), str(len(schema['static_methods']))]
    require(len(prefix) == PREFIX_WORDS, 'Internal fact prefix mismatch')
    lines += ['extern "C" const std::uint64_t CseVgui1AppColdFacts[] = {']
    lines += ['    ' + expression + ',' for expression in prefix + expressions]
    lines += ['};', 'extern "C" const std::uint64_t CseVgui1AppColdMetadataFacts[] = {',
              '    ProviderRoleSignature(), RoleFamily, CppProfileFamily, AppColdFamily', '};', '} }']
    if context['role'] == 'consumer':
        lines += ['static_assert(std::is_base_of<vgui::App, CDummyApp>::value, "Actual engine base");',
                  'static_assert(sizeof(CDummyApp) == sizeof(vgui::App), "Actual engine storage extent");',
                  'static_assert(alignof(CDummyApp) == alignof(vgui::App), "Actual engine storage alignment");',
                  'static_assert(std::is_trivially_destructible<CDummyApp>::value, "Actual engine destructor");',
                  'extern "C" vgui::App* CseVgui1AppColdBase(CDummyApp* value) {', '    return value;', '}']
    return '\n'.join(lines) + '\n'


def typed_constants(text, microsoft, symbol='CseVgui1AppColdFacts'):
    if not microsoft:
        match = re.search(r'^@' + re.escape(symbol)
                          + r' = [^\n]*constant \[(\d+) x i64\] \[(.*?)\], align', text, re.M)
        require(match is not None, 'Unknown/missing compiler typed constant report')
        values = [int(value) & ((1 << 64) - 1) for value in re.findall(r'i64 (-?\d+)', match[2])]
        require(len(values) == int(match[1]), 'Truncated compiler typed constants')
        return values
    lines = text.splitlines()
    starts = [i for i, line in enumerate(lines) if symbol in line
              and re.search(r'\b(?:DQ|DCQ)\b', line)]
    require(len(starts) == 1, 'Unknown/missing MSVC typed constant report')
    values = []
    for line in lines[starts[0]:]:
        match = re.search(r'\b(?:DQ|DCQ)\s+(0x[0-9a-fA-F]+|[0-9a-fA-F]+H)\s*$', line)
        if not match:
            break
        encoded = match[1]
        values.append(int(encoded[2:], 16) if encoded.startswith('0x') else int(encoded[:-1], 16))
    require(values, 'Empty MSVC typed constants')
    return values


def normalized_type(value):
    return re.sub(r'\s+', '', value.replace('class ', '').replace('struct ', ''))


def layouts(text, microsoft, schema, role):
    if microsoft:
        text = re.sub(r'Note: including file:[^\n]*\n', '', text)
    result = {}
    wanted = {normalized_type(value): name for name, value in schema['records'].items()}
    if role == 'consumer':
        wanted['CDummyApp'] = 'CDummyApp'
    if microsoft:
        matches = re.finditer(r'class ([^\n]+?)\s+size\((\d+)\):\n(.*?)(?=\n\n)', text, re.S)
    else:
        matches = re.finditer(r'\*\*\* Dumping AST Record Layout\n\s*0 \| class ([^\n]+)\n(.*?\[sizeof=(\d+),.*?align=(\d+),.*?\])', text, re.S)
    for match in matches:
        key = normalized_type(match[1])
        if key not in wanted:
            continue
        record = wanted[key]
        require(record not in result, 'Duplicate compiler layout report: ' + record)
        body = match[3] if microsoft else match[2]
        size = int(match[2] if microsoft else match[3])
        fields = []
        bases = []
        vptrs = []
        for line in body.splitlines():
            item = re.match(r'\s*(\d+)\s*\|( +)(.*)', line)
            if not item:
                continue
            offset = int(item[1])
            description = item[3]
            if microsoft:
                if description.startswith('|'):
                    continue
                base = re.fullmatch(r'\+--- \(base class ([\w:]+)\)', description)
                if base:
                    bases.append({'type': base[1], 'offset': offset})
                    continue
                if description == '{vfptr}':
                    vptrs.append(offset)
                    continue
                if description.startswith(('+---', '<alignment')):
                    continue
            else:
                if len(item[2]) != 3:
                    continue
                base = re.fullmatch(r'class ([\w:]+) \((?:primary )?base\)', description)
                if base:
                    bases.append({'type': base[1], 'offset': offset})
                    continue
                if description.endswith(' vtable pointer)'):
                    vptrs.append(offset)
                    continue
            name = re.search(r'(_\w+)\s*$', description)
            require(name is not None, 'Unknown compiler layout member: ' + description)
            fields.append({'name': name[1], 'offset': offset})
        if record == 'CDummyApp':
            require(bases == [{'type': 'vgui::App', 'offset': 0}] and not fields and not vptrs,
                    'Actual engine base/view drift')
        else:
            expected = [row['name'] for row in schema['fields'] if row['record'] == record]
            require([row['name'] for row in fields] == expected, 'Actual field order/count drift: ' + record)
            require(not bases and vptrs == ([0] if record == 'App' else []), 'Actual base/vptr drift: ' + record)
        result[record] = {'bytes': size, 'fields': fields, 'bases': bases, 'vptr_offsets': vptrs}
    require(set(result) == set(wanted.values()), 'Missing/unknown actual class layouts')
    if role == 'consumer':
        require(result['CDummyApp']['bytes'] == result['App']['bytes'], 'Actual engine complete storage drift')
    return result


def msvc_table_symbols(assembly):
    lines = assembly.splitlines()
    starts = [i for i, line in enumerate(lines) if '??_7App@vgui@@6B@' in line
              and re.search(r'\b(?:DD|DQ|DCQ)\b', line)]
    require(len(starts) == 1, 'Missing/unknown actual App physical symbol table')
    symbols = []
    for line in lines[starts[0] + 1:]:
        item = re.match(r'\s*(?:DD|DQ|DCQ)\s+(?:FLAT:)?\|?([^|,;\s]+)', line)
        if not item:
            break
        symbols.append(item[1])
    require(len(symbols) == 55, 'Actual full typed physical symbol count drift')
    return symbols


def physical_slots(text, assembly, microsoft, schema):
    if microsoft:
        # MSVC can splice /showIncludes diagnostics into a partial layout line.
        # Remove only the recognized diagnostic, then validate the complete row.
        text = re.sub(r'Note: including file:[^\n]*\n', '', text)
    methods = schema['methods']
    by_name = {}
    for row in methods:
        by_name.setdefault(row['name'], []).append(row)
    entries = []
    if microsoft:
        reports = re.findall(r'vgui::App::\$vftable@:\n(.*?)(?=\n\n)', text, re.S)
        require(len(reports) == 1, 'Missing/unknown actual App vftable report')
        names = re.findall(r'^\s*(\d+)\s*\| &vgui::App::(\w+)\s*$', reports[0], re.M)
        require(len(names) == 55, 'Actual virtual count/report drift')
        symbols = msvc_table_symbols(assembly)
        for (slot, name), symbol in zip(names, symbols):
            require(int(slot) == len(entries), 'Noncontiguous physical slots')
            require(name in by_name, 'Unknown actual virtual method: ' + name)
            choices = by_name[name]
            if len(choices) == 2:
                require(name == 'setMouseArena', 'Unknown overload group')
                if re.search(r'HHHH_N@Z$', symbol):
                    row = next(row for row in choices if row['arguments'].startswith('int'))
                elif re.search(r'(?:PAV|PEAV)Panel@2@@Z$', symbol):
                    row = next(row for row in choices if row['arguments'].startswith('vgui::Panel'))
                else:
                    raise common.EvidenceError('Unknown full overload symbol')
            else:
                row = choices[0]
                if name == 'main':
                    require(symbol in ('_purecall', '__purecall'), 'Unknown abstract App main slot')
                else:
                    require(symbol.startswith('?' + name + '@App@vgui@@'), 'Physical typed method owner/name drift')
            entries.append({'id': row['id'], 'slot': int(slot), 'name': name})
        adjustors = re.findall(r'vgui::App::(\w+) this adjustor: (-?\d+)', text)
        require(len(adjustors) == 55 and all(value == '0' for _, value in adjustors), 'Missing/nonzero App adjustor')
        require(sorted(name for name, _ in adjustors) == sorted(name for _, name in names), 'Adjustor method drift')
    else:
        report = None
        for actual in ('vgui::App', 'CDummyApp'):
            found = re.findall(r"Vtable for '" + actual + r"' \((\d+) entries\)\.(.*?)(?=\n\n)", text, re.S)
            if found:
                require(len(found) == 1 and int(found[0][0]) == 57, 'Actual vtable entry/report drift')
                report = found[0][1]
                break
        require(report is not None, 'Missing actual App/derived typed vtable report')
        rows = re.findall(r'^\s*(\d+) \| (.*)$', report, re.M)
        require(len(rows) == 57 and rows[0] == ('0', 'offset_to_top (0)'), 'Unknown vtable/base prefix')
        for slot, member in rows[2:]:
            match = re.fullmatch(r'(.*?)(?:vgui::App|CDummyApp)::(\w+)\((.*?)\)(?: \[pure\])?', member)
            require(match is not None and match[2] in by_name, 'Unknown actual full virtual type report')
            choices = [row for row in by_name[match[2]]
                       if normalized_type(row['arguments']).replace('vgui::', '')
                       == normalized_type(match[3]).replace('vgui::', '')]
            require(len(choices) == 1, 'Unknown overload arguments in actual physical table')
            row = choices[0]
            returns = [row['result']]
            if row['native_provider_int_variant']:
                returns.append('int')
            require(normalized_type(match[1]).replace('vgui::', '')
                    in [normalized_type(value).replace('vgui::', '') for value in returns],
                    'Unknown full virtual return type report')
            require(int(slot) - 2 == len(entries), 'Noncontiguous actual typed slots')
            entries.append({'id': row['id'], 'slot': int(slot) - 2, 'name': match[2]})
        require(not re.search(r"Thunks for '[^'\\n]*(?:vgui::App|CDummyApp)::", text)
                and 'VCall offset' not in report, 'Unexpected App virtual adjustor')
    expected = [row['id'] for row in methods]
    if microsoft:
        group = [index for index, row in enumerate(methods) if row['name'] == 'setMouseArena']
        require(len(group) == 2 and group[1] == group[0] + 1, 'Unknown semantic overload mapping')
        expected[group[0]], expected[group[1]] = expected[group[1]], expected[group[0]]
    require([row['id'] for row in entries] == expected, 'Actual physical method order drift')
    return entries


def constructor_shape(assembly, microsoft, role, architecture):
    if microsoft:
        symbol = r'\?\?0App@vgui@@(?:QAE|QEAA)@XZ'
        if role == 'provider':
            require(re.search(symbol + r'\|?\s+PROC', assembly), 'Missing actual no-argument App constructor definition')
        else:
            require(re.search(r'\b(?:call|bl)\s+\|?' + symbol, assembly), 'Missing actual imported App base-constructor call')
        return {'base_constructor': 1, 'call_profile': architecture, 'arguments': 0, 'native_this': True}
    pattern = r'@_ZN4vgui3AppC2Ev\(ptr[^\n)]*\)'
    require(re.search(pattern, assembly), 'Missing actual Itanium App C2 base constructor shape')
    if role == 'consumer':
        require(re.search(r'call [^\n]*ptr @_ZN4vgui3AppC2Ev\(ptr', assembly), 'Missing actual engine App base-constructor call')
    else:
        require(re.search(r'^define [^\n]*ptr @_ZN4vgui3AppC2Ev\(ptr', assembly, re.M), 'Missing actual App base-constructor definition')
    return {'base_constructor': 2, 'call_profile': architecture, 'arguments': 0, 'native_this': True}


def unpack_words(values, schema):
    require(len(values) > PREFIX_WORDS and values[0] == MAGIC and values[1] == FORMAT, 'Unknown typed fact prefix')
    prefix = values[:PREFIX_WORDS]
    require(prefix[2] == schema['contract_revision']
            and prefix[4:6] == [schema['runtime_version'], schema['descriptor_bytes']], 'Typed foundation/schema drift')
    require(prefix[6] in (1, 2, 3) and prefix[7] in (1, 2) and prefix[8] in (1, 2), 'Unknown compiler call profile')
    require(prefix[17:21] == [1, 0, 1, 1], 'Actual abstract/destructor/polymorphism drift')
    require(prefix[26:29] == [1, 1, 1], 'Actual RTTI/static storage profile drift')
    require(prefix[23:26] == [31, 55, sum(len(row['values']) for row in schema['enums'])], 'Typed schema count drift')
    at = PREFIX_WORDS
    fields = []
    for spec in schema['fields']:
        row = values[at:at + FIELD_WORDS]
        require(len(row) == FIELD_WORDS and row[0] == spec['id'], 'Typed field fact order/count drift')
        fields.append({'id': row[0], 'shape': row[1], 'declared_shape': row[2], 'bytes': row[3],
                       'alignment': row[4], 'rank': row[5], 'extents': row[6:8], 'scalar_signed': row[8],
                       'const': row[9], 'volatile': row[10], 'record': spec['record'], 'name': spec['name']})
        at += FIELD_WORDS
    static_fields = []
    for spec in schema['static_fields']:
        row = values[at:at + 4]
        require(len(row) == 4 and row[0] == spec['id'], 'Static storage type report drift')
        static_fields.append({'id': row[0], 'name': spec['name'], 'shape': row[1],
                              'bytes': row[2], 'alignment': row[3]})
        at += 4
    static_methods = []
    for spec in schema['static_methods']:
        require(values[at] == spec['id'], 'Static method type report drift')
        static_methods.append({'id': values[at], 'name': spec['name'], 'shape': values[at + 1]})
        at += 2
    methods = []
    for spec in schema['methods']:
        require(values[at] == spec['id'], 'Typed method fact order drift')
        methods.append({'id': values[at], 'shape': values[at + 1]})
        at += 2
    enums = []
    for enum_id, spec in enumerate(schema['enums'], 1):
        count = len(spec['values'])
        metadata = values[at:at + 6]
        require(metadata[0] == enum_id and metadata[1:3] == [4, 4] and metadata[5] == count,
                'Actual enum storage/count drift')
        actual = values[at + 6:at + 6 + count]
        require(actual == [row['expected'] for row in spec['values']], 'Actual enum value drift')
        enums.append(metadata + actual)
        at += 6 + count
    require(at == len(values), 'Unknown/trailing/truncated typed facts')
    return {'prefix': prefix, 'fields': fields, 'static_fields': static_fields,
            'rtti_enabled': prefix[26], 'static_methods': static_methods, 'methods': methods, 'enums': enums}


def semantic_words(facts):
    typed = facts['typed']
    # The compiler report's framing/version is provenance, not an ABI fact.
    words = [FAMILY] + typed['prefix'][2:]
    for row in typed['fields']:
        physical = next(field for field in facts['layouts'][row['record']]['fields'] if field['name'] == row['name'])
        words += [row['id'], common.fnv(list(physical['name'].encode('ascii'))),
                  physical['offset'], row['shape'], row['bytes'], row['alignment'],
                  row['rank']] + row['extents'] + [row['scalar_signed'], row['const'], row['volatile']]
    for row in typed['static_fields']:
        words += [row['id'], common.fnv(list(row['name'].encode('ascii'))),
                  row['shape'], row['bytes'], row['alignment']]
    for row in typed['static_methods']:
        words += [row['id'], common.fnv(list(row['name'].encode('ascii'))), row['shape']]
    for slot in facts['slots']:
        method = next(row for row in typed['methods'] if row['id'] == slot['id'])
        words += [slot['id'], common.fnv(list(slot['name'].encode('ascii'))), slot['slot'], method['shape'], 0]  # actual this-adjustor is separately checked zero.
    for row in typed['enums']:
        words += row
    for key in ('App', 'SurfaceDar', 'TickDar'):
        record = facts['layouts'][key]
        words += [record['bytes'], len(record['vptr_offsets'])] + record['vptr_offsets']
    words += [0, facts['constructor']['base_constructor'], facts['constructor']['call_profile'],
              facts['constructor']['arguments'], int(facts['constructor']['native_this'])]
    return words


def invalidate(output):
    for name in ('accepted.stamp', 'evidence.h', 'metadata.h', 'metadata.stamp'):
        (output / name).unlink(missing_ok=True)


def metadata_contexts(entries, manifest, options, inputs):
    """This production-shaped data mode cannot turn a copied-header fixture into an offer."""
    require(not options.fixture_only and manifest.get('compile_database'),
            'Metadata requires fresh actual owning database contexts, never fixture-only arguments')
    root = Path(__file__).resolve().parents[1]
    require(options.foundation.resolve() == root / 'src/public/cse_vgui1_app_cold_abi.h'
            and options.schema.resolve() == root / 'tools/vgui1_app_cold_evidence_schema.json',
            'Metadata requires the source-owned foundation/schema')
    expected = {('consumer', 'hw_engine', str(root / 'src/engine/vgui_intwrap.cpp')),
                ('consumer', 'swds', str(root / 'src/engine/vgui_intwrap.cpp')),
                ('provider', 'vgui', str(root / 'src/utils/vgui/lib-src/vgui/App.cpp'))}
    require(len(entries) == len(expected)
            and {(row['role'], row.get('target'), str(Path(row['source']).resolve())) for row in entries} == expected,
            'Metadata requires actual hw/sw App consumers and actual common VGUI App provider')
    require(not any('fixture_header_selection' in row for row in entries), 'Fixture header selection cannot publish')
    for path in (root / 'src/public/cse_vgui1_app_cold_metadata.h',
                 root / 'src/common/vgui1_app_cold_publisher.cpp'):
        inputs[str(path)] = common.file_digest(path)
    return root


def metadata_header(reference, provenance_digest, protocol):
    prefix = reference['typed']['prefix']
    require(prefix[8] == 2 and prefix[9] == 8, 'Metadata mode currently supports only actual Apple64 contexts')
    facts = reference['metadata_facts']
    require(len(facts) == 4 and facts[1:] == [0x4f4d0001, 0x4f4d0002, FAMILY],
            'Actual compiler metadata family mapping differs')
    profile = common.fnv(prefix[6:11] + prefix[21:23] + [prefix[26]])
    descriptor = [prefix[5], 1, prefix[4], prefix[6], prefix[9] * 8, prefix[7], 3, 0]
    require(all(value != 0 for value in (facts[0], profile, reference['signature'])), 'Empty compiler signature')
    lines = ['#pragma once', '#if !defined(CSE_VGUI1_APP_COLD_METADATA_PUBLISHER)',
             '#error Fresh AppCold metadata is only for the constant publisher', '#endif',
             '#include "' + protocol.as_posix() + '"',
             'namespace cse { namespace vgui1_app_cold_generated {',
             'constexpr bool MetadataReady = true;', 'constexpr bool ColdLoadReady = false;',
             'static_assert(abi::NativeArchitecture == ' + str(prefix[6])
             + ' && abi::NativeCompilerAbi == ' + str(prefix[7])
             + ' && abi::NativePointerBits == ' + str(prefix[9] * 8) + ', "Actual target profile differs");',
             'constexpr vgui1_app_cold_metadata::Digest Provenance{{'
             + ', '.join(str(value) for value in bytes.fromhex(provenance_digest)) + '}};',
             'constexpr abi::Descriptor Catalog{' + ', '.join(str(value) for value in descriptor) + ', {']
    for family, signature in zip(facts[1:], (facts[0], profile, reference['signature'])):
        lines.append('    {' + str(family) + ', 0, ' + str(signature) + 'ull},')
    lines += ['}};', '} }', '']
    return '\n'.join(lines)


def generate(options):
    output = options.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    invalidate(output)  # Never parse a previous receipt before revoking acceptance.
    failure = {'status': 'rejected', 'publisher_ready': False, 'cold_load_ready': False, 'metadata_ready': False}
    try:
        inputs = {str(Path(__file__).resolve()): common.file_digest(__file__),
                  str(Path(common.__file__).resolve()): common.file_digest(common.__file__)}
        for path in (options.schema, options.contexts, options.foundation):
            inputs[str(path.resolve())] = common.file_digest(path)
        schema = json.loads(options.schema.read_text())
        validate_schema(schema)
        manifest = json.loads(options.contexts.read_text())
        require(manifest['format'] == FORMAT, 'Unknown context manifest')
        require(options.fixture_only or 'compile_database' in manifest, 'Actual owning compile database required')
        entries = common.actual_context_entries(manifest, options.contexts, inputs)
        metadata_root = metadata_contexts(entries, manifest, options, inputs) if options.metadata else None
        contexts = []
        for entry in entries:
            require(entry['role'] in ('consumer', 'provider'), 'Unknown App owner context role')
            entry = dict(entry)
            # Legacy shared helper's dependency anchor; no filesystem interface is used here.
            entry['filesystem_abi_header'] = str(options.foundation.parent / 'cse_module_abi.h')
            contexts.append(common.prepare_context(entry, inputs))
        require(len(contexts) >= 2 and {row['role'] for row in contexts} == {'consumer', 'provider'},
                'Both actual owner/consumer contexts are required')
        require(len({row['name'] for row in contexts}) == len(contexts), 'Duplicate context name')
        identities = {}
        prepared = []
        closure = {}
        environment = {key: os.environ[key] for key in common.ENVIRONMENT_INPUTS if key in os.environ}
        for context in contexts:
            inputs[context['source']] = common.file_digest(context['source'])
            work = output / 'contexts' / context['name']
            work.mkdir(parents=True, exist_ok=True)
            probe = work / 'probe.cpp'
            common.atomic(probe, probe_source(context, schema, options.foundation.resolve()))
            prefix = common.compilation_prefix(context)
            if context['microsoft']:
                prefix = [value for value in prefix if value.lower() not in ('/c', '-c')]
            actual_ids, configurations = common.compiler_identity(context, prefix, work)
            identities.update(actual_ids)
            for path in configurations:
                inputs[str(path.resolve())] = common.file_digest(path)
            before = work / 'preprocess.d'
            command = prefix + (['/E', '/showIncludes', str(probe)] if context['microsoft'] else
                                ['-E', '-MD', '-MF', str(before), '-MT', str(work / 'probe.o'), str(probe)])
            process = common.run(command, context['directory'], work / 'preprocess.log')
            require(process.returncode == 0, 'Actual preprocessing failed: ' + context['name'])
            paths = common.dependency_paths(context, process.stdout + process.stderr, before, probe)
            hashes = {str(path): common.file_digest(path) for path in paths}
            if metadata_root:
                expected_header = metadata_root / ('src/utils/vgui/include/VGUI_App.h'
                    if context['role'] == 'consumer' else 'src/utils/vgui/lib-src/vgui/include/VGUI_App.h')
                declaring_headers = {path for path in paths if path.name == 'VGUI_App.h'}
                require(declaring_headers == {expected_header},
                        'Metadata cannot use forced/copied alternate App declarations: ' + context['name'])
                require(not any(path in (output / 'metadata.h', output / 'evidence.h') for path in paths),
                        'Generated evidence must not be injected into actual declaring probes')
                require(not any(b'namespace vgui1_app_cold_generated' in path.read_bytes()
                                or b'namespace vgui1_app_cold_evidence' in path.read_bytes() for path in paths),
                        'No generated AppCold header may be injected into actual declaring probes')
            closure.update(hashes)
            prepared.append((context, work, probe, prefix, paths, hashes,
                             common.digest_bytes(process.stdout + process.stderr)))
        common.verify_inputs(inputs)
        common.verify_inputs(identities)
        common.verify_inputs(closure)  # Includes headers unique to earlier contexts.
        observations = []
        compiles = []
        for context, work, probe, prefix, paths, hashes, token_hash in prepared:
            depfile = work / 'compiler.d'
            if context['microsoft']:
                command = prefix + ['/d1reportAllClassLayout', '/showIncludes', '/FAcs',
                                    '/Fa' + str(work / 'probe.asm'), '/c', str(probe),
                                    '/Fo' + str(work / 'probe.obj'), '/Fd' + str(work / 'probe.pdb')]
                assembly_path = work / 'probe.asm'
            else:
                command = prefix + ['-Xclang', '-fdump-vtable-layouts', '-Xclang', '-fdump-record-layouts',
                                    '-MD', '-MF', str(depfile), '-MT', str(work / 'probe.o'),
                                    '-S', '-emit-llvm', str(probe), '-o', str(work / 'probe.ll')]
                assembly_path = work / 'probe.ll'
            process = common.run(command, context['directory'], work / 'compiler.log')
            compiles.append({'context': context['name'], 'arguments': command, 'exit': process.returncode})
            require(process.returncode == 0, 'Actual declaring CPP compile failed: ' + context['name'])
            after = common.dependency_paths(context, process.stdout + process.stderr, depfile, probe)
            require(after == paths, 'Include closure changed during actual replay')
            common.verify_inputs(hashes)
            text = (process.stdout + process.stderr).decode(errors='replace').replace('\r\n', '\n')
            assembly = assembly_path.read_text(errors='replace')
            typed = unpack_words(typed_constants(assembly, context['microsoft']), schema)
            facts = {'context': context['name'], 'role': context['role'], 'typed': typed,
                     'metadata_facts': typed_constants(assembly, context['microsoft'], 'CseVgui1AppColdMetadataFacts'),
                     'layouts': layouts(text, context['microsoft'], schema, context['role']),
                     'slots': physical_slots(text, assembly, context['microsoft'], schema),
                     'constructor': constructor_shape(assembly, context['microsoft'], context['role'], typed['prefix'][6]),
                     'preprocessed_sha256': token_hash}
            for key, size_index in [('App', 11), ('SurfaceDar', 13), ('TickDar', 15)]:
                require(facts['layouts'][key]['bytes'] == typed['prefix'][size_index], 'Typed/physical storage disagreement')
            facts['semantic_words'] = semantic_words(facts)
            facts['signature'] = common.fnv(facts['semantic_words'])
            common.atomic(work / 'facts.json', json.dumps(facts, indent=2))
            observations.append(facts)
        common.verify_inputs(inputs)
        common.verify_inputs(identities)
        common.verify_inputs(closure)
        common.atomic(output / 'observations.json', json.dumps(observations, indent=2))
        common.atomic(output / 'compiles.json', json.dumps(compiles, indent=2))
        reference = observations[0]
        differences = sorted({a['id'] for other in observations[1:]
                              for a, b in zip(reference['typed']['methods'], other['typed']['methods'])
                              if a != b})
        failure['typed_method_differences'] = differences
        require(all(row['signature'] == reference['signature'] for row in observations),
                'Actual complete AppCold typed/physical owner-consumer disagreement')
        require(all(row['metadata_facts'] == reference['metadata_facts'] for row in observations),
                'Actual compiler dependency-edge role facts disagree')
        provenance = {'format': FORMAT, 'contexts': contexts, 'compilers': identities,
                      'inputs': inputs, 'dependency_hashes': closure, 'effective_environment': environment,
                      'compiles': compiles, 'fixture_only': options.fixture_only,
                      'preprocessed_hashes': {row['context']: row['preprocessed_sha256'] for row in observations}}
        digest = common.digest_bytes(common.stable(provenance))
        signature = reference['signature']
        header = '#pragma once\n#if !defined(CSE_VGUI1_APP_COLD_COMPONENT_EVIDENCE)\n'
        header += '#error AppCold evidence is component-only; publisher/preload integration is not qualified\n#endif\n'
        header += '#include "' + options.foundation.resolve().as_posix() + '"\n'
        header += 'namespace cse { namespace vgui1_app_cold_evidence {\n'
        header += 'constexpr unsigned long long Signature = ' + str(signature) + 'ull;\n'
        profile = common.fnv(reference['typed']['prefix'][6:11] + reference['typed']['prefix'][21:23]
                             + [reference['typed']['prefix'][26]])
        header += 'constexpr unsigned long long ProfileSignature = ' + str(profile) + 'ull;\n'
        header += 'constexpr abi::Descriptor Catalog = abi::Describe({\n'
        header += '    {vgui1_app_cold_abi::RoleFamily, 0, vgui1_app_cold_abi::ProviderRoleSignature()},\n'
        header += '    {vgui1_app_cold_abi::CppProfileFamily, 0, ProfileSignature},\n'
        header += '    {vgui1_app_cold_abi::AppColdFamily, 0, Signature}\n});\n'
        header += 'constexpr bool PublisherReady = false;\nconstexpr bool ColdLoadReady = false;\n} }\n'
        common.atomic(output / 'evidence.h', header)
        if metadata_root:
            common.atomic(output / 'metadata.h', metadata_header(reference, digest,
                metadata_root / 'src/public/cse_vgui1_app_cold_metadata.h'))
        provenance.update(status='accepted_component_only', signature=signature, input_digest=digest,
                          publisher_ready=False, cold_load_ready=False,
                          metadata_ready=bool(metadata_root),
                          metadata_header_sha256=common.file_digest(output / 'metadata.h') if metadata_root else None,
                          generated_header_sha256=common.file_digest(output / 'evidence.h'))
        common.verify_inputs(inputs)
        common.verify_inputs(identities)
        common.verify_inputs(closure)
        common.atomic(output / 'receipt.json', json.dumps(provenance, indent=2))
        if metadata_root:
            common.atomic(output / 'metadata.stamp', digest + '\n')
        common.atomic(output / 'accepted.stamp', digest + '\n')
        print('ACCEPTED component-only actual AppCold evidence:', signature,
              '; publisher/cold_load_ready=false; no probe execution')
    except Exception as error:
        invalidate(output)
        failure['error'] = str(error)
        common.atomic(output / 'failure.json', json.dumps(failure, indent=2))
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    command = commands.add_parser('generate')
    for name in ('schema', 'contexts', 'foundation', 'output'):
        command.add_argument('--' + name, type=Path, required=True)
    command.add_argument('--fixture-only', action='store_true')
    command.add_argument('--metadata', action='store_true',
                         help='Also emit unselected constant metadata from the three actual owning contexts only')
    options = parser.parse_args()
    try:
        generate(options)
    except Exception as error:
        print('REJECTED:', error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
