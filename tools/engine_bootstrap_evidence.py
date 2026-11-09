#!/usr/bin/env python3
"""Fail-closed, compile-only owned bootstrap ABI evidence. Never execute generated code."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile

PIPELINE_FORMAT = 1
ENVIRONMENT_INPUTS = ('INCLUDE', 'CPATH', 'CPLUS_INCLUDE_PATH', 'C_INCLUDE_PATH', 'SDKROOT',
                      'MACOSX_DEPLOYMENT_TARGET', 'CL', '_CL_')


class EvidenceError(RuntimeError):
    pass


def require(condition, message):
    if not condition:
        raise EvidenceError(message)


def digest_bytes(value):
    return hashlib.sha256(value).hexdigest()


def file_digest(path):
    return digest_bytes(Path(path).read_bytes())


def stable(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':')).encode()


def atomic(path, content):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    data = content.encode() if isinstance(content, str) else content
    if path.is_file() and path.read_bytes() == data:
        return
    temporary = path.with_name(path.name + '.tmp')
    temporary.write_bytes(data)
    temporary.replace(path)


def split_windows(command):
    require(os.name == 'nt', 'Windows command strings require the real Windows argument parser; use arguments elsewhere')
    api = ctypes.windll.shell32
    api.CommandLineToArgvW.argtypes = [ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_int)]
    api.CommandLineToArgvW.restype = ctypes.POINTER(ctypes.c_wchar_p)
    count = ctypes.c_int()
    pointer = api.CommandLineToArgvW(command, ctypes.byref(count))
    require(bool(pointer), 'CommandLineToArgvW failed')
    try:
        return [pointer[index] for index in range(count.value)]
    finally:
        ctypes.windll.kernel32.LocalFree(ctypes.cast(pointer, ctypes.c_void_p))


def decode_response(data):
    if data.startswith((b'\xff\xfe', b'\xfe\xff')):
        return data.decode('utf-16')
    return data.decode('utf-8-sig')


def expand_response(arguments, directory, microsoft, inputs, stack=()):
    result = []
    require(len(stack) <= 8, 'Response nesting exceeds the bounded depth')
    for argument in arguments:
        if not argument.startswith('@'):
            result.append(argument)
            continue
        path = Path(argument[1:])
        if not path.is_absolute():
            path = directory / path
        path = path.resolve()
        require(path not in stack, 'Response file cycle: ' + str(path))
        data = path.read_bytes()
        require(len(data) <= 1024 * 1024, 'Response file exceeds the bounded size')
        inputs[str(path)] = digest_bytes(data)
        text = decode_response(data)
        values = split_windows('response ' + text)[1:] if microsoft else shlex.split(text)
        # Compiler response paths, including nested @files, are relative to its cwd.
        result += expand_response(values, directory, microsoft, inputs, stack + (path,))
    return result


def run(command, directory, log=None, environment=None):
    child = os.environ.copy()
    if environment:
        child.update(environment)
    child.pop('CL', None)
    child.pop('_CL_', None)
    if os.name == 'nt':
        child['VSLANG'] = '1033'
    process = subprocess.run(command, cwd=directory, env=child, capture_output=True, timeout=180)
    if log:
        Path(log).write_bytes(process.stdout + process.stderr)
    return process


def prepare_context(entry, responses):
    directory = Path(entry['directory']).resolve()
    source = Path(entry.get('source', entry.get('file', '')))
    if not source.is_absolute():
        source = directory / source
    source = source.resolve()
    arguments = entry.get('arguments')
    if arguments is None:
        command = entry['command']
        arguments = split_windows(command) if os.name == 'nt' else shlex.split(command)
    require(arguments, 'Empty compiler context')
    compiler_name = Path(arguments[0]).name.lower()
    microsoft = compiler_name in ('cl', 'cl.exe')
    if microsoft:
        arguments = [arguments[0]] + split_windows('cl ' + os.environ.get('CL', ''))[1:] + arguments[1:]
        arguments += split_windows('cl ' + os.environ.get('_CL_', ''))[1:]
    arguments = expand_response(arguments, directory, microsoft, responses)
    compiler = shutil.which(arguments[0]) or arguments[0]
    compiler = Path(compiler).resolve()
    require(compiler.is_file(), 'Compiler executable missing: ' + str(compiler))
    arguments[0] = str(compiler)
    name = entry.get('name', entry.get('target', 'context'))
    require(re.fullmatch(r'[A-Za-z_][A-Za-z0-9_-]*', name), 'Invalid context name')
    require(source.is_file(), 'Actual source missing: ' + str(source))
    return {'name': name, 'role': entry['role'], 'directory': str(directory), 'source': str(source),
            'arguments': arguments, 'microsoft': microsoft,
            'filesystem_abi_header': str(Path(entry['filesystem_abi_header']).resolve())}


def actual_context_entries(manifest, manifest_path, inputs):
    database_name = manifest.get('compile_database')
    if not database_name:
        return manifest['contexts']
    path = Path(database_name)
    if not path.is_absolute():
        path = manifest_path.parent / path
    path = path.resolve()
    inputs[str(path)] = file_digest(path)
    database = json.loads(path.read_text())
    result = []
    for requested in manifest['contexts']:
        require('arguments' not in requested and 'command' not in requested,
                'Do not override an actual database context with separately invented options')
        source = Path(requested['source']).resolve()
        target = requested['target']
        require(re.fullmatch(r'[A-Za-z_][A-Za-z0-9_-]*', target), 'Invalid context target selector')
        matches = []
        for entry in database:
            candidate = Path(entry['file'])
            if not candidate.is_absolute():
                candidate = Path(entry['directory']) / candidate
            output = entry.get('output', entry.get('command', ''))
            if candidate.resolve() == source and re.search(r'CMakeFiles[/\\]' + re.escape(target) + r'\.dir', output):
                matches.append(entry)
        require(len(matches) == 1, 'Actual compiler database target/source is missing or ambiguous: ' + target)
        entry = dict(matches[0])
        entry.update(requested)
        entry.setdefault('name', target)
        result.append(entry)
    return result


def compilation_prefix(context):
    arguments = context['arguments']
    microsoft = context['microsoft']
    source = os.path.normcase(context['source'])
    result = [arguments[0]]
    skip = False
    for argument in arguments[1:]:
        if skip:
            skip = False
            continue
        candidate = Path(argument)
        if not candidate.is_absolute():
            candidate = Path(context['directory']) / candidate
        if os.path.normcase(str(candidate.resolve())) == source:
            continue
        lower = argument.lower()
        if microsoft:
            if argument in ('/Fo', '/Fd', '/Fe', '/Fa', '-Fo', '-Fd', '-Fe', '-Fa'):
                skip = True
                continue
            if argument.startswith(('/Fo', '/Fd', '/Fe', '/Fa', '-Fo', '-Fd', '-Fe', '-Fa')):
                continue
        else:
            if argument in ('-o', '-MF', '-MT', '-MQ', '-MJ'):
                skip = True
                continue
            if argument in ('-c', '-MD', '-MMD', '-MP', '-M', '-MM'):
                continue
        result.append(argument)
    return result


def probe_source(context, schema, foundation_overlay):
    role = schema['roles'][context['role']]
    lines = ['#include "' + Path(context['source']).as_posix() + '"',
             '#include "' + foundation_overlay.as_posix() + '"',
             'constexpr auto EvidenceTypedContext = ' + role['typed_validation'] + ';',
             'static_assert(EvidenceTypedContext.tableCount == 3, "Typed bootstrap context");',
             'static_assert(cse::abi::RuntimeVersion == 10, "Qualified runtime envelope");',
             'static_assert(sizeof(cse::abi::Descriptor) == 288, "Qualified descriptor envelope");']
    lines.append('static_assert(cse::engine_bootstrap_abi::MethodCount == '
                 + str(len(schema['header_method_mapping'])) + ', "Complete typed schema mapping");')
    for mapping in schema['header_method_mapping']:
        index = mapping['header_index']
        interface_id = schema['interfaces'][mapping['interface']]['identifier']
        lines.append('static_assert(static_cast<unsigned long long>(cse::engine_bootstrap_abi::Methods['
                     + str(index) + '].interfaceId) == ' + str(interface_id) + ', "Schema interface ID");')
        lines.append('static_assert(cse::engine_bootstrap_abi::Methods[' + str(index) + '].ordinal == '
                     + str(mapping['ordinal']) + ', "Schema physical ordinal mapping");')
    for name in role['interfaces']:
        require(re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', name), 'Unclassified interface selector')
        lines += ['extern "C" IBaseInterface* EvidenceBase_' + name + '(' + name + '* value) {',
                  '    return value;', '}']
        for method in schema['interfaces'][name]['methods']:
            specification = schema['probes'][method]
            invocation = 'value->' + method + '(' + ', '.join(specification['arguments']) + ')'
            lines += ['extern "C" ' + specification['result'] + ' EvidenceCall_' + name + '_' + method
                      + '(' + name + '* value) {',
                      '    ' + ('' if specification['result'] == 'void' else 'return ') + invocation + ';', '}']
    return '\n'.join(lines) + '\n'


def validate_schema(schema):
    wanted = {(name, method) for name, interface in schema['interfaces'].items()
              for method in interface['methods']}
    mappings = schema['header_method_mapping']
    actual = [(row['interface'], row['method']) for row in mappings]
    require(set(actual) == wanted and len(actual) == len(wanted), 'Missing/duplicate semantic method mapping')
    require(len({row['header_index'] for row in mappings}) == len(mappings), 'Duplicate typed schema selector')
    for row in mappings:
        require(schema['interfaces'][row['interface']]['methods'].index(row['method']) == row['ordinal'],
                'Schema order diverges from its frozen typed semantic selector')


def parse_layout(text, microsoft, wanted, schema):
    text = text.replace('\r\n', '\n')
    text = re.sub(r'Note: including file:[^\n]*\n', '', text)
    tables = []
    if microsoft:
        pattern = r'(?P<type>[\w:]+)::\$vftable@:\n(?P<body>.*?)(?=\n\n)'
        destructors = 1
    else:
        pattern = r"VTable indices for '(?P<type>[\w:]+)' \(\d+ entries\)\.\n(?P<body>.*?)(?=\n\n)"
        destructors = 2
    for match in re.finditer(pattern, text, re.S):
        name = match['type']
        if name not in wanted:
            continue
        entries = []
        for line in match['body'].splitlines():
            item = re.match(r'\s*(\d+)\s*\|\s*(.*)', line)
            if not item:
                continue
            member = item[2]
            if microsoft:
                method = member.split('::')[-1].strip()
                if '{dtor}' in method:
                    method = 'destructor'
            else:
                method = member.split('::')[-1].split('(')[0]
                if method.startswith('~'):
                    method = 'destructor'
            entries.append({'slot': int(item[1]), 'method': method})
        expected = ['destructor'] * destructors + schema['interfaces'][name]['methods']
        require(entries == [{'slot': index, 'method': method} for index, method in enumerate(expected)],
                'Physical method order/count drift: ' + name)
        # These three interfaces have exactly one direct base, at offset zero.
        if microsoft:
            record = re.search(r'class ' + re.escape(name) + r'\s+size\(\d+\):\n(?P<body>.*?)(?=\n\n)', text, re.S)
            require(record is not None, 'Missing real class layout: ' + name)
            bases = re.findall(r'^\s*(\d+)\s*\|\s*\+--- \(base class ([\w:]+)\)', record['body'], re.M)
            require(bases == [('0', schema['interfaces'][name]['base'])], 'Base/view layout drift: ' + name)
            for method in schema['interfaces'][name]['methods']:
                adjustors = re.findall(re.escape(name + '::' + method) + r' this adjustor: (-?\d+)', text)
                require(adjustors and set(adjustors) == {'0'}, 'This-adjustor drift: ' + name + '::' + method)
        else:
            record = re.search(r'\| class ' + re.escape(name) + r'\n(?P<body>.*?\[sizeof=.*?\])', text, re.S)
            require(record is not None, 'Missing real class layout: ' + name)
            bases = re.findall(r'^\s*(\d+)\s*\|   class ([\w:]+) \((?:primary )?base\)', record['body'], re.M)
            require(bases == [('0', schema['interfaces'][name]['base'])], 'Base/view layout drift: ' + name)
            require(not re.search(r'Thunk.*' + re.escape(name), match['body']), 'Unexpected interface thunk')
        tables.append({'interface': name, 'identifier': schema['interfaces'][name]['identifier'],
                       'destructor_slots': destructors, 'base_offset': 0, 'this_adjustor': 0, 'entries': entries})
    require({row['interface'] for row in tables} == set(wanted) and len(tables) == len(wanted),
            'Missing/duplicate/unknown compiler interface reports')
    return tables


def dependency_paths(context, stdout, depfile, probe):
    values = {Path(context['source']).resolve(), probe.resolve(),
              Path(context['filesystem_abi_header']).resolve()}
    if context['microsoft']:
        text = stdout.decode(errors='replace').replace('\r\n', '\n')
        found = re.findall(r'Note: including file:\s*([^\n]+)', text)
        require(found, 'MSVC include closure missing; cannot accept cached evidence')
        values.update(Path(value.strip()).resolve() for value in found)
    else:
        require(depfile.is_file(), 'Compiler dependency output missing')
        text = depfile.read_text().replace('\\\n', '')
        require(':' in text, 'Malformed dependency output')
        for value in shlex.split(text.split(':', 1)[1]):
            path = Path(value)
            if not path.is_absolute():
                path = Path(context['directory']) / path
            values.add(path.resolve())
    require(all(path.is_file() for path in values), 'Dependency closure contains missing files')
    return values


def fnv(facts):
    value = 14695981039346656037
    for number in facts:
        require(isinstance(number, int) and 0 <= number < 2 ** 64, 'Invalid semantic ABI fact')
        for byte in number.to_bytes(8, 'little'):
            value ^= byte
            value = value * 1099511628211 & ((1 << 64) - 1)
    return value


def constructor_model(requirements, inputs):
    state = requirements.get('state')
    require(state in ('fixture-only', 'unresolved', 'modeled'), 'Unknown constructor requirements state')
    edges = requirements['edges']
    if state != 'modeled':
        require(not edges, 'Constructor edges require explicit consumer/provider identity modeling')
        return {'state': state, 'cold_load_ready': False, 'actual_constructor_safety': False}
    images = {}
    for record in requirements.get('images', []):
        name = record['name']
        require(name not in images, 'Duplicate owned image identity')
        path = Path(record['path']).resolve()
        actual = file_digest(path)
        require(actual == record['sha256'], 'Staged image identity changed: ' + name)
        inputs[str(path)] = actual
        require(isinstance(record.get('provided'), dict) and isinstance(record.get('required'), dict),
                'Missing image-owned provided/required contract maps')
        images[name] = record
    require(images, 'Modeled constructor edges need owned image identities')
    observed = set()
    for edge in edges:
        consumer = edge['consumer']
        provider = edge['provider']
        family = str(edge['family'])
        require(consumer in images and provider in images, 'Unknown constructor-edge image')
        # Read the ENGINE/consumer's own requirement, never a launcher surrogate.
        required = images[consumer]['required'].get(provider, {}).get(family)
        supplied = images[provider]['provided'].get(family)
        require(isinstance(required, int) and isinstance(supplied, int) and required != 0 and required == supplied,
                'Consumer-owned constructor contract mismatch: ' + consumer + ' -> ' + provider)
        observed.add((consumer, provider, family))
    for consumer, image in images.items():
        for provider, contracts in image['required'].items():
            for family in contracts:
                require((consumer, provider, family) in observed, 'Missing declared constructor requirement edge')
    return {'state': 'matched-model-only', 'image_names': sorted(images), 'edge_count': len(edges),
            'cold_load_ready': False, 'actual_constructor_safety': False,
            'limit': 'No actual VGUI1 catalog or reviewed preload reader; no image was loaded'}


def semantic_signature(context_facts, schema):
    canonical = {}
    for rows in context_facts.values():
        for row in rows:
            name = row['interface']
            require(name not in canonical or canonical[name] == row, 'Owner/caller physical ABI disagreement: ' + name)
            canonical[name] = row
    require(set(canonical) == set(schema['interfaces']), 'Incomplete offered/callback ABI graph')
    words = [schema['contract_revision'], schema['runtime_version'], schema['descriptor_bytes']]
    for name in sorted(canonical, key=lambda key: schema['interfaces'][key]['identifier']):
        row = canonical[name]
        words += [row['identifier'], row['destructor_slots'], row['base_offset'], row['this_adjustor']]
        for entry in row['entries']:
            if entry['method'] == 'destructor':
                semantic = 0
            else:
                selected = [mapping for mapping in schema['header_method_mapping']
                            if mapping['interface'] == name and mapping['method'] == entry['method']]
                require(len(selected) == 1, 'Unclassified semantic method ID')
                semantic = selected[0]['header_index'] + 1
            words += [semantic, entry['slot']]
    return fnv(words)


def compiler_identity(context, prefix, work):
    compiler = Path(prefix[0])
    identities = {str(compiler): file_digest(compiler)}
    if context['microsoft']:
        for name in ['c1xx.dll', 'c2.dll']:
            path = compiler.parent / name
            require(path.is_file(), 'Missing actual MSVC frontend/backend identity: ' + str(path))
            identities[str(path)] = file_digest(path)
        return identities, []
    trace = run(prefix + ['-###', '-fsyntax-only', context['source']], context['directory'])
    text = (trace.stdout + trace.stderr).decode(errors='replace')
    require('clang' in text.lower() and '-cc1' in text, 'Unsupported compiler/report family')
    inputs = []
    for line in text.splitlines():
        if '"-cc1"' in line:
            arguments = shlex.split(line)
            path = Path(arguments[0]).resolve()
            require(path.is_file(), 'Compiler frontend identity is unavailable')
            identities[str(path)] = file_digest(path)
        if line.startswith('Configuration file:'):
            path = Path(line.split(':', 1)[1].strip()).resolve()
            inputs.append(path)
    return identities, inputs


def invalidate(output):
    for name in ['accepted.stamp', 'evidence.h']:
        (output / name).unlink(missing_ok=True)


def generated_header(physical, input_digest, module_header):
    return '''#pragma once
+// Generated compiler evidence. Never use a manually pinned substitute.
+#include "MODULE_HEADER"
+
+namespace cse { namespace bootstrap_evidence {
+constexpr unsigned long long PhysicalSignature = PHYSICALull;
+constexpr bool ModelOnly = true;
+constexpr bool PublisherReady = false;
+constexpr bool ColdLoadReady = false;
+constexpr char InputDigest[] = "INPUT_DIGEST";
+
+constexpr abi::Descriptor Bind(abi::Descriptor value) {
+    for (std::size_t index = 0; index < value.tableCount && index < abi::MaxTables; ++index) {
+        if (value.tables[index].identifier == 0x4f4d0100u) {
+            value.tables[index].signature = abi::LayoutHash({value.tables[index].signature, PhysicalSignature});
+        }
+    }
+    return value;
+}
+} }
+
+#if !defined(CSE_ENGINE_BOOTSTRAP_FIXTURE_EVIDENCE)
+#error Real constructor requirements and reviewed preload binding are not ready; fixture evidence cannot publish.
+#endif
+'''.replace('\n+', '\n').replace('MODULE_HEADER', module_header.as_posix()).replace('PHYSICAL', str(physical)).replace('INPUT_DIGEST', input_digest)


def preprocess_context(context, schema, foundation_source, output):
    work = output / 'contexts' / context['name']
    work.mkdir(parents=True, exist_ok=True)
    foundation = work / 'cse_engine_bootstrap_abi.h'
    atomic(foundation, foundation_source.read_bytes())
    atomic(work / 'cse_filesystem_abi.h', '#pragma once\n#include "'
           + Path(context['filesystem_abi_header']).as_posix() + '"\n')
    probe = work / 'probe.cpp'
    atomic(probe, probe_source(context, schema, foundation))
    prefix = compilation_prefix(context)
    preliminary = work / 'preprocess.d'
    if context['microsoft']:
        command = prefix + ['/E', '/showIncludes', str(probe)]
    else:
        command = prefix + ['-E', '-MD', '-MF', str(preliminary), '-MT', str(work / 'probe.o'),
                            str(probe), '-o', str(work / 'preprocessed.i')]
    process = run(command, context['directory'])
    require(process.returncode == 0, 'Actual preprocessing failed: ' + context['name'])
    paths = dependency_paths(context, process.stdout + process.stderr, preliminary, probe)
    paths.update([foundation, work / 'cse_filesystem_abi.h'])
    hashes = {str(path): file_digest(path) for path in sorted(paths)}
    if context['microsoft']:
        tokens = re.sub(rb'Note: including file:[^\n]*\n', b'', process.stdout + process.stderr)
    else:
        tokens = (work / 'preprocessed.i').read_bytes()
        (work / 'preprocessed.i').unlink()
    token_hash = digest_bytes(tokens)
    atomic(work / 'preprocess-inputs.json', json.dumps({'arguments': command,
           'dependency_hashes': hashes, 'preprocessed_sha256': token_hash, 'exit': process.returncode}, indent=2))
    return work, probe, prefix, paths, hashes, token_hash


def verify_inputs(inputs):
    require(all(Path(path).is_file() and file_digest(path) == value for path, value in inputs.items()),
            'Source/schema/options/compiler input changed during compiler replay')


def write_depfile(path, output, closure, inputs, identities):
    if path:
        def escape(value):
            return str(value).replace('\\', '/').replace(' ', '\\ ').replace('#', '\\#').replace(':', '\\:')
        dependencies = set(closure) | set(inputs) | set(identities)
        atomic(path, escape(output / 'accepted.stamp') + ': '
               + ' '.join(escape(value) for value in sorted(dependencies)) + '\n')


def generate(options):
    output = options.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    (output / 'accepted.stamp').unlink(missing_ok=True)
    try:
        previous = None
        if (output / 'receipt.json').is_file():
            previous = json.loads((output / 'receipt.json').read_text())
        schema = json.loads(options.schema.read_text())
        manifest = json.loads(options.contexts.read_text())
        require(schema['format'] == PIPELINE_FORMAT and manifest['format'] == PIPELINE_FORMAT, 'Unknown input format')
        require(schema['runtime_version'] == 10 and schema['descriptor_bytes'] == 288, 'Envelope drift requires review')
        validate_schema(schema)
        constructors = manifest.get('constructor_requirements')
        require(isinstance(constructors, dict) and isinstance(constructors.get('edges'), list),
                'Missing explicit constructor requirements')
        require(options.fixture_only, 'Production readiness blocked: actual VGUI1 catalog and reviewed preload reader missing')
        responses = {}
        database_inputs = {}
        entries = actual_context_entries(manifest, options.contexts.resolve(), database_inputs)
        contexts = [prepare_context(entry, responses) for entry in entries]
        require({entry['role'] for entry in contexts} == set(schema['roles']), 'Incomplete actual compiler contexts')
        for entry in contexts:
            require(entry['role'] in schema['roles'], 'Unknown actual declaring role')
        inputs = {str(options.schema.resolve()): file_digest(options.schema),
                  str(options.contexts.resolve()): file_digest(options.contexts),
                  str(options.foundation.resolve()): file_digest(options.foundation),
                  str(Path(__file__).resolve()): file_digest(__file__)}
        inputs.update(responses)
        inputs.update(database_inputs)
        identity_model = constructor_model(constructors, inputs)
        identities = {}
        for context in contexts:
            inputs[context['source']] = file_digest(context['source'])
            inputs[context['filesystem_abi_header']] = file_digest(context['filesystem_abi_header'])
            identity, configurations = compiler_identity(context, compilation_prefix(context), output)
            identities.update(identity)
            for path in configurations:
                inputs[str(path)] = file_digest(path)
        initial = {'format': PIPELINE_FORMAT, 'contexts': contexts, 'inputs': inputs, 'compiler_identities': identities,
                   'environment_inputs': {key: os.environ[key] for key in ENVIRONMENT_INPUTS if key in os.environ},
                   'fixture_only': options.fixture_only, 'constructor_requirements': constructors,
                   'constructor_identity_model': identity_model}
        initial_digest = digest_bytes(stable(initial))
        prepared_contexts = {}
        preprocessed = {}
        current_closure = {}
        for context in contexts:
            prepared = preprocess_context(context, schema, options.foundation, output)
            prepared_contexts[context['name']] = prepared
            current_closure.update(prepared[4])
            preprocessed[context['name']] = prepared[5]
        verify_inputs(inputs)
        verify_inputs(identities)
        verify_inputs(current_closure)
        if previous and previous.get('initial_digest') == initial_digest and previous.get('status') == 'accepted_fixture_only':
            header = output / 'evidence.h'
            if (current_closure == previous.get('dependency_hashes')
                    and preprocessed == previous.get('preprocessed_hashes') and header.is_file()
                    and file_digest(header) == previous.get('generated_header_sha256')):
                previous['cache_hit'] = True
                atomic(output / 'receipt.json', json.dumps(previous, indent=2))
                atomic(output / 'accepted.stamp', previous['input_digest'] + '\n')
                write_depfile(options.depfile, output, current_closure, inputs, identities)
                print('ACCEPTED fixture evidence (cache hit after actual preprocessing); publisher/cold readiness false')
                return
        facts = {}
        dependencies = set()
        compiles = []
        for context in contexts:
            work, probe, prefix, before_paths, before_hashes, token_hash = prepared_contexts[context['name']]
            depfile = work / 'compiler.d'
            if context['microsoft']:
                command = prefix + ['/d1reportAllClassLayout', '/showIncludes', '/c', str(probe),
                                    '/Fo' + str(work / 'probe.obj'), '/Fd' + str(work / 'probe.pdb')]
            else:
                command = prefix + ['-Xclang', '-fdump-vtable-layouts', '-Xclang', '-fdump-record-layouts',
                                    '-MD', '-MF', str(depfile), '-MT', str(work / 'probe.o'),
                                    '-S', '-emit-llvm', str(probe), '-o', str(work / 'probe.ll')]
            process = run(command, context['directory'], work / 'compiler.log')
            compiles.append({'context': context['name'], 'arguments': command, 'exit': process.returncode})
            require(process.returncode == 0, 'Actual owning compile failed: ' + context['name'])
            text = (process.stdout + process.stderr).decode(errors='replace')
            facts[context['name']] = parse_layout(text, context['microsoft'],
                                                   schema['roles'][context['role']]['interfaces'], schema)
            after_paths = dependency_paths(context, process.stdout + process.stderr, depfile, probe)
            after_paths.update([work / 'cse_engine_bootstrap_abi.h', work / 'cse_filesystem_abi.h'])
            require(before_paths == after_paths, 'Include closure changed during compiler replay')
            require(all(path.is_file() and file_digest(path) == before_hashes[str(path)] for path in after_paths),
                    'Source/include input changed during compiler replay')
            dependencies.update(after_paths)
        require(all(Path(path).is_file() and file_digest(path) == value for path, value in inputs.items()),
                'Source/schema/options input changed during compiler replay')
        require(all(Path(path).is_file() and file_digest(path) == value for path, value in identities.items()),
                'Compiler identity changed during replay')
        physical = semantic_signature(facts, schema)
        closure = {str(path): file_digest(path) for path in sorted(dependencies)}
        full_digest = digest_bytes(stable({'initial': initial_digest, 'dependency_hashes': closure, 'preprocessed_hashes': preprocessed}))
        module_header = Path(contexts[0]['filesystem_abi_header']).parent / 'cse_module_abi.h'
        require(module_header.is_file(), 'Actual qualified module envelope header missing')
        header_text = generated_header(physical, full_digest, module_header)
        atomic(output / 'evidence.h', header_text)
        receipt = dict(initial, status='accepted_fixture_only', cache_hit=False, initial_digest=initial_digest,
                       input_digest=full_digest, dependency_hashes=closure, preprocessed_hashes=preprocessed, physical_signature=physical,
                       compiler_facts=facts, compiles=compiles, publisher_ready=False, cold_load_ready=False,
                       generated_header_sha256=file_digest(output / 'evidence.h'))
        atomic(output / 'receipt.json', json.dumps(receipt, indent=2))
        atomic(output / 'metadata.json', json.dumps({'format': 1, 'runtime_version': 10, 'descriptor_bytes': 288,
               'physical_signature': physical, 'publisher_ready': False, 'cold_load_ready': False,
               'constructor_requirements': constructors,
               'constructor_identity_model': identity_model,
               'not_ready_reasons': ['actual VGUI1 consumer/provider catalog missing', 'reviewed pre-load reader/binding absent']}, indent=2))
        atomic(output / 'accepted.stamp', full_digest + '\n')
        write_depfile(options.depfile, output, closure, inputs, identities)
        print('ACCEPTED fixture evidence; actual physical signature', physical, '; publisher/cold readiness false')
    except Exception as error:
        invalidate(output)
        atomic(output / 'failure.json', json.dumps({'status': 'rejected', 'error': str(error),
               'publisher_ready': False, 'cold_load_ready': False}, indent=2))
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest='command', required=True)
    command = subparsers.add_parser('generate')
    command.add_argument('--schema', type=Path, required=True)
    command.add_argument('--contexts', type=Path, required=True)
    command.add_argument('--foundation', type=Path, required=True)
    command.add_argument('--output', type=Path, required=True)
    command.add_argument('--depfile', type=Path)
    command.add_argument('--fixture-only', action='store_true')
    options = parser.parse_args()
    try:
        generate(options)
    except (EvidenceError, OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        print('REJECTED:', error, file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
