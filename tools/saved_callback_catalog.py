"""Generate eager typed save callbacks from the CMake-selected source inventory.

Lexical declaration resolution is deliberately bounded; unknown or virtual
assignments fail rather than emitting an address cast. Compilation of each
fragment in its original translation unit proves access and callback signature.
"""
from pathlib import Path
import os
import collections
import re

KINDS = {
    'Think': {'field': 'm_pfnThink', 'owner': 'CBaseEntity', 'type': 'void (CBaseEntity::*)()', 'typedef': 'BASEPTR'},
    'Touch': {'field': 'm_pfnTouch', 'owner': 'CBaseEntity', 'type': 'void (CBaseEntity::*)(CBaseEntity*)', 'typedef': 'ENTITYFUNCPTR'},
    'Use': {'field': 'm_pfnUse', 'owner': 'CBaseEntity', 'type': 'void (CBaseEntity::*)(CBaseEntity*, CBaseEntity*, USE_TYPE, float)', 'typedef': 'USEPTR'},
    'Blocked': {'field': 'm_pfnBlocked', 'owner': 'CBaseEntity', 'type': 'void (CBaseEntity::*)(CBaseEntity*)', 'typedef': 'ENTITYFUNCPTR'},
    'MoveDone': {'field': 'm_pfnCallWhenMoveDone', 'owner': 'CBaseToggle', 'type': 'void (CBaseToggle::*)()', 'typedef': None},
}

def clean(text):
    pattern = r'/\*.*?\*/|//[^\n]*|"(?:[^"\\]|\\.)*"|\'(?:[^\'\\]|\\.)*\''
    return re.sub(pattern, lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0]), text, flags=re.S)

def compile_guard(text, position, include_guard=None):
    stack = []
    for match in re.finditer(r'^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b([^\n]*)', text[:position], re.M):
        directive = match[1]
        expression = match[2].strip()
        if directive in ('if', 'ifdef', 'ifndef'):
            if directive == 'ifdef':
                expression = 'defined(' + expression + ')'
            elif directive == 'ifndef':
                expression = '!defined(' + expression + ')'
            stack.append({'seen': [expression], 'active': expression})
        elif directive in ('else', 'elif') and stack:
            frame = stack[-1]
            previous = ' || '.join(frame['seen'])
            frame['active'] = '!(' + previous + ')'
            if directive == 'elif':
                frame['active'] += ' && (' + expression + ')'
                frame['seen'].append(expression)
        elif directive == 'endif' and stack:
            stack.pop()
    if include_guard and stack and stack[0]['active'] == '!defined(' + include_guard + ')':
        stack.pop(0)
    if len(stack) == 1:
        return stack[0]['active']
    return ' && '.join('(' + frame['active'] + ')' for frame in stack)


def location(REPO, path, text, position):
    match = re.match(r'\s*#ifndef\s+(\w+)\b', text) if path.suffix == '.h' else None
    include_guard = match[1] if match and re.search(r'^\s*#define\s+' + match[1] + r'\b', text, re.M) else None
    return {'path': str(path.relative_to(REPO)), 'line': text[:position].count('\n') + 1,
            'guard': compile_guard(text, position, include_guard)}

def inventory(root, game, target, sources, *, headers_root=None):
    REPO = root
    compiled = set(sources)
    hroot = headers_root or REPO / 'src' / ('dlls' if game == 'hl' else 'cstrike/dlls')
    headers = set(hroot.rglob('*.h'))
    classes = {}
    class_redefinitions = []
    call_sites = []
    direct_assignments = []
    entity_factories = []
    save_metadata = []
    function_fields = []
    for path in sorted(compiled | headers):
        original = path.read_text(encoding='utf-8', errors='replace')
        text = clean(original)
        for match in re.finditer(r'\bclass\s+(\w+)\s*(?::\s*([^;{]+))?\s*\{', text):
            name = match[1]
            start = match.end()
            end = start
            depth = 1
            while end < len(text) and depth:
                depth += (text[end] == '{') - (text[end] == '}')
                end += 1
            body = text[start:end - 1]
            depth = 0
            flattened = []
            for character in body:
                if character == '}':
                    depth -= 1
                flattened.append(character if depth == 0 else ('\n' if character == '\n' else ' '))
                if character == '{':
                    depth += 1
            flat = ''.join(flattened)
            accesses = [(a.start(), a[1]) for a in re.finditer(r'\b(public|protected|private)\s*:', flat)]
            members = {}
            for member in re.finditer(
                r'([^;{}]*?)\b(\w+)\s*\([^;{}]*?\)\s*(?:const\s*)?(?:override\s*)?[;{]', flat):
                member_position = member.start(2)
                access = next((value for pos, value in reversed(accesses) if pos < member_position), 'private')
                declaration_start = member.start()
                # Remove access-label text absorbed by the permissive declaration prefix.
                last_access = next((pos for pos, value in reversed(accesses) if pos < member_position), None)
                if last_access is not None and last_access >= declaration_start:
                    declaration_start = flat.index(':', last_access) + 1
                declaration = original[start + declaration_start:start + member.end()].strip()
                members[member[2]] = {
                    'declaring_class': name,
                    'method': member[2],
                    'access': access,
                    'explicit_virtual_or_override': bool(re.search(r'\b(virtual|override)\b', member[0])),
                    'export_marker': bool(re.search(r'\bEXPORT\b', member[0])),
                    'declaration': declaration,
                    **location(REPO, path, text, start + member_position),
                }
            record = {
                'name': name,
                'parents': re.findall(r'(?:public|protected|private)\s+(\w+)', match[2] or ''),
                'members': members,
                **location(REPO, path, text, match.start(1)),
            }
            if name in classes:
                class_redefinitions.append({'name': name, 'first': classes[name], 'later': record})
            classes[name] = record
        if path.name != 'cbase.h':
            for match in re.finditer(r'\bSet(Think|Touch|Use|Blocked|MoveDone)\s*\(\s*([^\)]*)\)', text):
                expression = re.sub(r'\s+', '', match[2])
                qualified = re.fullmatch(r'&(\w+)::(\w+)', expression)
                call_sites.append({
                    'kind': match[1],
                    'expression': expression,
                    'is_null': expression in ('NULL', '0', 'nullptr'),
                    'qualified_class': qualified[1] if qualified else None,
                    'method': qualified[2] if qualified else None,
                    'source_category': 'compiled_source' if path in compiled else 'game_local_header',
                    **location(REPO, path, text, match.start()),
                })
            for match in re.finditer(r'\bm_pfn(?:Think|Touch|Use|Blocked|CallWhenMoveDone)\s*=(?!=)[^;]+', text):
                direct_assignments.append({'text': match[0], **location(REPO, path, text, match.start())})
        if path in compiled:
            for match in re.finditer(r'\bLINK_ENTITY_TO_CLASS\s*\(\s*([^,]+),\s*([^\)]+)', text):
                entity_factories.append({
                    'entity_classname': match[1].strip(), 'cpp_class': match[2].strip(),
                    **location(REPO, path, text, match.start()),
                })
            for match in re.finditer(r'TYPEDESCRIPTION\s+(\w+)\s*::\s*m_SaveData', text):
                save_metadata.append({'cpp_class': match[1], **location(REPO, path, text, match.start())})
            for match in re.finditer(r'DEFINE_FIELD\s*\(\s*(\w+)\s*,\s*(\w+)\s*,\s*FIELD_FUNCTION\s*\)', text):
                function_fields.append({'cpp_class': match[1], 'field': match[2], **location(REPO, path, text, match.start())})

    def resolve(owner, method, trail=()):
        if owner in trail or owner not in classes:
            return None
        cls = classes[owner]
        if method in cls['members']:
            return cls['members'][method]
        for parent in cls['parents']:
            found = resolve(parent, method, trail + (owner,))
            if found:
                return found
        return None

    def is_virtual(owner, method, trail=()):
        if owner in trail or owner not in classes:
            return False
        cls = classes[owner]
        if method in cls['members'] and cls['members'][method]['explicit_virtual_or_override']:
            return True
        return any(is_virtual(parent, method, trail + (owner,)) for parent in cls['parents'])

    grouped = collections.defaultdict(list)
    for site in call_sites:
        if site['qualified_class']:
            grouped[(site['kind'], site['expression'], site['qualified_class'], site['method'])].append(site)
    expressions = []
    canonical = {}
    unresolved = []
    for (kind, expression, owner, method), sites in sorted(grouped.items()):
        declaration = resolve(owner, method)
        if declaration is None:
            unresolved.append({'kind': kind, 'expression': expression, 'sites': sites})
            continue
        canonical_name = method + '@' + declaration['declaring_class']
        record = {
            'kind': kind,
            'type': KINDS[kind],
            'expression': expression,
            'expression_class': owner,
            'canonical_name': canonical_name,
            'declaration': declaration,
            'virtual_in_hierarchy': is_virtual(owner, method),
            'sites': sites,
        }
        expressions.append(record)
        key = (kind, canonical_name)
        if key not in canonical:
            canonical[key] = {
                'kind': kind, 'type': KINDS[kind], 'canonical_name': canonical_name,
                'declaration': declaration, 'virtual_in_hierarchy': record['virtual_in_hierarchy'],
                'source_expressions': [], 'sites': [],
            }
        canonical[key]['source_expressions'].append(expression)
        canonical[key]['sites'] += sites
    canonical = [canonical[key] for key in sorted(canonical)]
    private = [entry for entry in canonical if entry['declaration']['access'] != 'public']
    summary = {
        'compiled_source_files': len(compiled), 'game_local_headers_scanned': len(headers),
        'set_call_sites': len(call_sites), 'nonnull_set_call_sites': sum(not x['is_null'] for x in call_sites),
        'null_set_call_sites': sum(x['is_null'] for x in call_sites),
        'qualified_kind_expressions': len(grouped), 'canonical_kind_methods': len(canonical),
        'canonical_declaring_classes': len({x['declaration']['declaring_class'] for x in canonical}),
        'canonical_by_kind': dict(collections.Counter(x['kind'] for x in canonical)),
        'compiled_files_with_qualified_assignments': len({s['path'] for x in expressions for s in x['sites'] if s['source_category'] == 'compiled_source'}),
        'nonpublic_methods': len(private),
        'nonpublic_classes': len({x['declaration']['declaring_class'] for x in private}),
        'virtual_assigned_methods': sum(x['virtual_in_hierarchy'] for x in canonical),
        'unresolved_qualified_expressions': len(unresolved),
        'entity_factory_sites': len(entity_factories),
        'entity_factory_cpp_classes': len({x['cpp_class'] for x in entity_factories}),
        'save_metadata_classes': len({x['cpp_class'] for x in save_metadata}),
        'function_fields': len(function_fields),
        'direct_callback_field_assignments_outside_cbase_header': len(direct_assignments),
    }
    return {
        'schema': 1, 'game': game, 'target': target, 'summary': summary,
        'scope': 'Current generated target source paths plus every game-local header; comments removed; conditional branches retained; lexical class/declaration resolution, not compiler AST proof.',
        'expressions': expressions, 'canonical_callbacks': canonical,
        'all_set_sites': call_sites,
        'unqualified_nonnull_sites': [x for x in call_sites if not x['is_null'] and not x['qualified_class']],
        'unresolved': unresolved, 'nonpublic_callbacks': private,
        'function_fields': function_fields, 'entity_factories': entity_factories,
        'save_metadata': save_metadata, 'class_definitions': classes,
        'class_redefinitions': class_redefinitions,
        'direct_assignments': direct_assignments,
    }


TYPES = {'Think': 'BASEPTR', 'Touch': 'ENTITYFUNCPTR', 'Blocked': 'ENTITYFUNCPTR',
         'Use': 'USEPTR', 'MoveDone': 'CseMoveDonePointer'}


def write_generated(path, text):
    if not path.is_file() or path.read_text() != text:
        path.write_text(text)


def generate(root, target, sources):
    game = {'hl_server': 'hl', 'cs_server': 'cs'}[target]
    result = inventory(root, game, target, sources)
    if result['unresolved']:
        raise ValueError('Unresolved save callback: ' + repr(result['unresolved']))
    for callback in result['canonical_callbacks']:
        if callback['virtual_in_hierarchy']:
            raise ValueError('Cannot register virtual save callback: ' + callback['canonical_name'])
    unknown = result['unqualified_nonnull_sites']
    if unknown or result['direct_assignments']:
        raise ValueError('Uncatalogued save callback assignment: ' + repr(unknown or result['direct_assignments']))

    game_root = root / 'src' / ('dlls' if game == 'hl' else 'cstrike/dlls')
    destination = game_root / 'generated_saved_callbacks'
    destination.mkdir(parents=True, exist_ok=True)
    # One specialization per declaring class, inside the translation unit that
    # owns a private class or already sees its public header declaration.
    owners = collections.defaultdict(list)
    for callback in result['canonical_callbacks']:
        owners[callback['declaration']['declaring_class']].append(callback)
    hosts = collections.defaultdict(list)
    for owner, callbacks in sorted(owners.items()):
        declaration = root / callbacks[0]['declaration']['path']
        if declaration.suffix == '.cpp':
            host = declaration
        else:
            sites = [root / site['path'] for callback in callbacks for site in callback['sites']
                     if site['source_category'] == 'compiled_source']
            host = min(sites) if sites else declaration.with_suffix('.cpp')
        if host not in sources:
            raise ValueError('Save callback host is not a retained source: ' + str(host))
        hosts[host].append((owner, callbacks))

    functions = []
    expected = set()
    for host, classes in sorted(hosts.items()):
        leaf = host.stem + '.inc'
        if leaf in expected:
            raise ValueError('Duplicate save callback host basename: ' + leaf)
        expected.add(leaf)
        include = os.path.relpath(destination / leaf, host.parent).replace(os.sep, '/')
        if '#include "' + include + '"' not in host.read_text():
            raise ValueError('Missing eager callback fragment include in ' + str(host))
        function = 'CseRegisterCallbacks_' + host.stem
        functions.append(function)
        lines = ['// Generated by tools/generate_runtime_sources.py; do not edit.',
                 '#ifndef CLIENT_DLL', '#include "game_saved_callbacks.h"', '']
        for owner, callbacks in classes:
            owner_guard = result['class_definitions'][owner]['guard']
            if owner_guard:
                lines.append('#if ' + owner_guard)
            lines.extend(['template<>', 'struct CseSavedCallbackAccess<' + owner + '> {',
                          '    static bool Register() {', '        bool valid = true;'])
            for callback in callbacks:
                kind = callback['kind']
                name = callback['canonical_name']
                method = callback['declaration']['method']
                # Registration follows the method's availability, not its
                # assignment sites: e.g. a Debug-only trip beam still names a
                # method compiled in Release and must remain restorable there.
                guard = callback['declaration']['guard']
                if guard:
                    lines.append('#if ' + guard)
                lines.append(f'        valid = CseRegister{kind}("{name}",')
                lines.append(f'            static_cast<{TYPES[kind]}>(&{owner}::{method})) && valid;')
                if guard:
                    lines.append('#endif')
            lines.extend(['        return valid;', '    }', '};', ''])
            if owner_guard:
                lines.append('#endif')
        lines.extend(['bool ' + function + '() {', '    bool valid = true;'])
        for owner, callbacks in classes:
            owner_guard = result['class_definitions'][owner]['guard']
            if owner_guard:
                lines.append('#if ' + owner_guard)
            lines.append('    valid = CseSavedCallbackAccess<' + owner + '>::Register() && valid;')
            if owner_guard:
                lines.append('#endif')
        lines.extend(['    return valid;', '}', '#endif', ''])
        write_generated(destination / leaf, '\n'.join(lines))
    lines = ['// Generated by tools/generate_runtime_sources.py; do not edit.']
    lines += ['bool ' + function + '();' for function in functions]
    lines += ['', 'static bool CseRegisterGameCallbacks() {', '    bool valid = true;']
    lines += ['    valid = ' + function + '() && valid;' for function in functions]
    lines += ['    return valid;', '}', '']
    write_generated(destination / 'catalog.inc', '\n'.join(lines))
    for stale in destination.glob('*.inc'):
        if stale.name not in expected | {'catalog.inc'}:
            stale.unlink()
    return {'callbacks': len(result['canonical_callbacks']), 'hosts': len(hosts), 'inventory': result}
