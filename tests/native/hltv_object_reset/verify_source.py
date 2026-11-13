#!/usr/bin/env python3
"""Verify the three approved source edits and unchanged declarations/serialization/lookup implementations."""
import argparse
import hashlib
import json
from pathlib import Path
import re

from extract_regions import function

ROOT = Path(__file__).resolve().parents[3]


def method(text, signature):
    start = text.index(signature)
    body = function(text, signature)
    opening = text.index('{', start)
    return text[start:opening] + body


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--before', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    prefix = Path('src/HLTV')
    old_network = (options.before / prefix / 'Core/Network.cpp').read_text()
    network = (ROOT / prefix / 'Core/Network.cpp').read_text()
    original_reset = '\tmemset( &m_LocalAddress, 0, sizeof( m_LocalAddress ) );'
    assert old_network.count(original_reset) == 1
    assert network == old_network.replace(original_reset, '    m_LocalAddress.Clear();')
    old_address = (options.before / prefix / 'common/NetAddress.cpp').read_text()
    address = (ROOT / prefix / 'common/NetAddress.cpp').read_text()
    assert address == old_address.replace(method(old_address, 'NetAddress::NetAddress()'),
                                          'NetAddress::NetAddress() {\n    Clear();\n}')
    old_world = (options.before / prefix / 'Core/World.cpp').read_text()
    world = (ROOT / prefix / 'Core/World.cpp').read_text()
    construction = '''    if (deltaCache != nullptr) {
        for (int index = 0; index < maxCacheIndex; ++index) {
            new (&deltaCache[index]) deltaCache_t();
        }
    }
'''
    assert function(world, 'bool World::Init(').count(construction) == 1
    assert function(world, 'bool World::Init(').replace(construction, '') == function(old_world, 'bool World::Init(')
    old_shutdown = function(old_world, 'void World::ShutDown()')
    new_shutdown = function(world, 'void World::ShutDown()')
    old_release = '''\tif ( deltaCache != NULL )
\t{
\t\tfree( deltaCache );
\t\tdeltaCache = NULL;
\t}'''
    new_release = '''    if (deltaCache != nullptr) {
        for (int index = 0; index < maxCacheIndex; ++index) {
            deltaCache[index].~deltaCache_t();
        }
        free(deltaCache);
        deltaCache = nullptr;
    }'''
    assert new_shutdown.replace(new_release, old_release) == old_shutdown
    expected = old_world.replace('#include <cstring>\n', '#include <cstring>\n#include <new>\n')
    for signature in ['bool World::Init(', 'void World::ShutDown()', 'void World::ClearEntityCache()']:
        expected = expected.replace(method(old_world, signature), method(world, signature))
    assert expected == world, 'other World source bytes changed'
    unchanged = []
    for name in ['Core/Network.h', 'Core/World.h', 'common/NetAddress.h', 'common/Bitbuffer.h', 'common/BitBuffer.cpp']:
        relative = prefix / name
        data = (ROOT / relative).read_bytes()
        assert data == (options.before / relative).read_bytes(), 'dependency/header changed: ' + name
        unchanged.append({'path': str(relative), 'sha256': hashlib.sha256(data).hexdigest()})
    receipt = {'network_only_payload_clear': True, 'address_only_constructor_payload_clear': True,
               'world_only_three_approved_methods_and_new_include': True,
               'world_init_only_adds_immediate_placement_construction': True,
               'world_shutdown_only_adds_typed_destruction_before_existing_free': True,
               'headers_and_bitbuffer_unchanged': unchanged,
               'windows_selected_invariance': 'Not claimed: these are approved shared lifetime corrections.'}
    options.output.write_text(json.dumps(receipt, indent=2))
    print('PASS exact source boundaries, unchanged headers/types/BitBuffer and all other algorithms')


if __name__ == '__main__':
    main()
