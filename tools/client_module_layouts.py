"""Generate complete owned engine/client C boundary facts from the retained declarations.

The emitted expressions are compiled in each actual owner/consumer type environment.
Function members hash their compiler-resolved prototypes in addition to storage layout.
"""
from pathlib import Path
import argparse
import json
import re

# Pointer-only engine internals are opaque here. In particular model_t's BSP arrays
# are not dereferenced by either retained client; cache_user_t arrays are shared.
FAMILIES = {
    'EngineFunctions': [('cl_enginefunc_t', 'engine/APIProxy.h')],
    'ClientFunctions': [('cldll_func_t', 'engine/APIProxy.h')],
    'NestedFunctions': [(name, 'common/' + header) for name, header in [
        ('triangleapi_t', 'triangleapi.h'), ('efx_api_t', 'r_efx.h'), ('event_api_t', 'event_api.h'),
        ('demo_api_t', 'demo_api.h'), ('net_api_t', 'net_api.h'), ('IVoiceTweak', 'ivoicetweak.h'),
        ('engine_studio_api_t', 'r_studioint.h'), ('r_studio_interface_t', 'r_studioint.h')]],
    'ExchangeFunctions': [(name, 'engine/APIProxy.h') for name in
        ('cldll_func_dst_t', 'cl_enginefunc_dst_t', 'modfuncs_t', 'engdata_t', 'modshelpers_t', 'modchelpers_t')],
    'ClientRecords': [(name, header) for name, header in [
        ('SCREENINFO', 'engine/cdll_int.h'), ('client_data_t', 'engine/cdll_int.h'),
        ('client_sprite_t', 'engine/cdll_int.h'), ('hud_player_info_t', 'engine/cdll_int.h'),
        ('color24', 'common/const.h'), ('wrect_t', 'cl_dll/wrect.h'), ('cvar_t', 'common/cvardef.h'), ('cmdalias_t', 'engine/APIProxy.h'),
        ('kbutton_t', 'cl_dll/kbutton.h'), ('entity_state_t', 'common/entity_state.h'),
        ('clientdata_t', 'common/entity_state.h'), ('local_state_t', 'common/entity_state.h'),
        ('weapon_data_t', 'common/weaponinfo.h'), ('ref_params_t', 'common/ref_params.h'),
        ('usercmd_t', 'common/usercmd.h'), ('event_args_t', 'common/event_args.h'),
        ('mstudioevent_t', 'common/studio_event.h'), ('client_textmessage_t', 'common/Sequence.h'),
        ('sequenceCommandLine_s', 'common/Sequence.h'), ('sequenceEntry_s', 'common/Sequence.h'),
        ('sentenceEntry_s', 'common/Sequence.h'), ('netadr_t', 'common/netadr.h'),
        ('net_adrlist_t', 'common/net_api.h'), ('net_response_t', 'common/net_api.h'),
        ('net_status_t', 'common/net_api.h'), ('screenfade_t', 'common/screenfade.h'),
        ('con_nprint_t', 'common/con_nprint.h'), ('cl_entity_t', 'common/cl_entity.h'),
        ('mouth_t', 'common/cl_entity.h'), ('latchedvars_t', 'common/cl_entity.h'),
        ('position_history_t', 'common/cl_entity.h'), ('efrag_t', 'common/cl_entity.h'),
        ('TEMPENTITY', 'common/r_efx.h'), ('particle_t', 'common/particledef.h'),
        ('BEAM', 'common/beamdef.h'), ('dlight_t', 'common/dlight.h')]],
    'MovementRecords': [(name, header) for name, header in [
        ('playermove_t', 'cstrike/pm_shared/pm_defs.h'), ('physent_t', 'pm_shared/pm_defs.h'),
        ('pmtrace_t', 'common/pmtrace.h'), ('pmplane_t', 'common/pmtrace.h'),
        ('movevars_t', 'pm_shared/pm_movevars.h'), ('plane_t', 'common/const.h'), ('trace_t', 'common/const.h')]],
    'ModelRecords': [(name, header) for name, header in [
        ('model_t', 'common/com_model.h'), ('hull_t', 'common/com_model.h'),
        ('cache_user_t', 'common/com_model.h'), ('alight_t', 'common/com_model.h'),
        ('player_info_t', 'common/com_model.h'), ('resource_t', 'engine/custom.h'),
        ('customization_t', 'engine/custom.h')]]}


def clean(text):
    text = re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S)
    return re.sub(r'^\s*#[^\n]*', '', text, flags=re.M)


def fields(text, name):
    text = clean(text)
    alias = re.search(r'\btypedef\s+struct\s+(\w+)\s+' + re.escape(name) + r'\s*;', text)
    tag = alias[1] if alias else name
    for match in re.finditer(r'\bstruct\s*(\w+)?\s*\{', text):
        begin = match.end()
        end = begin
        depth = 1
        while depth:
            depth += (text[end] == '{') - (text[end] == '}')
            end += 1
        tail = text[end:text.index(';', end)].strip()
        if tail != name and match[1] != tag:
            continue
        body = text[begin:end - 1]
        if '{' in body or '}' in body:
            raise ValueError('Nested declaration requires an explicit shared contract: ' + name)
        result = []
        for statement in body.split(';'):
            statement = statement.strip()
            if not statement:
                continue
            callback = re.search(r'\(\s*\*\s*(\w+)\s*\)', statement)
            if callback:
                result.append(callback[1])
                continue
            for member in statement.split(','):
                found = re.search(r'\b(\w+)\s*(?:\[[^\]]+\]\s*)*$', member)
                if not found or '(' in member or ')' in member or ':' in member:
                    raise ValueError('Unsupported member in ' + name + ': ' + statement)
                result.append(found[1])
        if not result or len(result) != len(set(result)):
            raise ValueError('Empty or duplicate contract members: ' + name)
        return result
    raise ValueError('Missing actual shared declaration: ' + name)


def generate(root, output=None, check=False):
    output = output or root / "src/common/generated_client_module_layouts.h"
    sdk = root / 'src'
    lines = ['// Generated by tools/client_module_layouts.py; do not edit.', '#pragma once',
             '#include "cse_client_abi.h"', '', 'namespace cse { namespace abi {', '']
    catalog = {}
    for family, types in FAMILIES.items():
        hashes = []
        for name, header in types:
            members = fields((sdk / header).read_bytes().decode('latin1'), name)
            prefix = name in ('player_info_t', 'playermove_t')
            extent = f'offsetof({name}, {members[-1]}) + sizeof((({name}*)nullptr)->{members[-1]})'
            lines += [f'// {header}: {len(members)} actual exposed fields.',
                      f'constexpr std::size_t ClientActualSize_{name} = sizeof({name});']
            if prefix:
                lines += [f'constexpr std::size_t ClientPrefixSize_{name} = {extent};',
                          f'static_assert(sizeof({name}) >= ClientPrefixSize_{name}, "Shared prefix exceeds owner");',
                          f'constexpr std::size_t ClientPrefixAlignment_{name} = ClientRequiredAlignment({{']
                lines += [f'    alignof(decltype((({name}*)nullptr)->{member})),' for member in members]
                lines += ['});',
                          f'static_assert(alignof({name}) >= ClientPrefixAlignment_{name},',
                          '    "Owner alignment cannot support its shared prefix");']
            lines += [f'constexpr std::uint64_t ClientLayout_{name} = LayoutHash({{',
                      f'    {"ClientPrefixSize_" + name if prefix else "sizeof(" + name + ")"},',
                      f'    {"ClientPrefixAlignment_" + name if prefix else "alignof(" + name + ")"},']
            for member in members:
                access = f'(({name}*)nullptr)->{member}'
                lines += [f'    ClientTextHash("{name}.{member}"), offsetof({name}, {member}), sizeof({access}),',
                          f'    alignof(decltype({access})),',
                          f'    ClientMemberType<decltype({access})>::Hash(),']
            lines += ['});', '']
            hashes.append(f'ClientLayout_{name}')
            catalog[name] = {'header': header, 'fields': members, 'prefix': prefix, 'family': family}
        versions = {'ClientFunctions': ['CLDLL_INTERFACE_VERSION'],
                    'NestedFunctions': ['TRI_API_VERSION', 'EVENT_API_VERSION', 'STUDIO_INTERFACE_VERSION']}
        lines += [f'constexpr std::uint64_t ClientFamily_{family} = LayoutHash({{',
                  '    ' + ',\n    '.join(versions.get(family, []) + hashes), '});', '']
    lines += ['constexpr Descriptor DescribeClientBoundary(ClientRole role) {', '    return Describe({',
              '        {static_cast<std::uint32_t>(ClientContract::Role), 0, static_cast<std::uint64_t>(role)},',
              '        {static_cast<std::uint32_t>(ClientContract::CProfile), 0, ClientCProfile()},']
    for family in FAMILIES:
        lines += [f'        {{static_cast<std::uint32_t>(ClientContract::{family}), 0, ClientFamily_{family}}},']
    lines += ['    });', '}', '',
              'constexpr Descriptor ClientModuleDescriptor = DescribeClientBoundary(ClientRole::Client);',
              'constexpr Descriptor EngineClientDescriptor = DescribeClientBoundary(ClientRole::Engine);',
              'static_assert(ClientModuleDescriptor.tableCount <= MaxTables, "Client catalog exceeds descriptor capacity");',
              '', '} }', '']
    content = '\n'.join(lines)
    if check:
        if not output.exists() or output.read_text() != content:
            raise ValueError('Generated client ABI contract is stale: ' + str(output))
    else:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(content)
    return catalog


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--catalog', type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    output = args.output or root / 'src/common/generated_client_module_layouts.h'
    catalog = generate(root, output, args.check)
    if args.catalog:
        args.catalog.write_text(json.dumps(catalog, indent=2) + '\n')
