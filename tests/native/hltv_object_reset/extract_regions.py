#!/usr/bin/env python3
"""Bind exact affected source regions to actual SDK record types; never replace class declarations."""
import argparse
import hashlib
import json
import re
from pathlib import Path


def function(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        if text[end] == '{':
            depth += 1
        elif text[end] == '}':
            depth -= 1
        end += 1
    return text[opening:end]


def extract(source_root, output):
    world_path = source_root / 'GoldSrc-SDK/HLTV/Core/World.cpp'
    network_path = source_root / 'GoldSrc-SDK/HLTV/Core/Network.cpp'
    world = world_path.read_text()
    network = network_path.read_text()
    init = function(world, 'bool World::Init(')
    allocate = init[init.index('\tframeCache = '):init.index('\tReset();')]
    shutdown = function(world, 'void World::ShutDown()')
    release_start = re.search(r'[ \t]*if\s*\(\s*deltaCache\s*!=\s*(?:NULL|nullptr)\s*\)', shutdown).start()
    release = shutdown[release_start:shutdown.index('\tBaseSystemModule::ShutDown();')]
    network_init = function(network, 'bool Network::Init(')
    reset_address = network_init[network_init.index('\tm_Sockets.Init();') + len('\tm_Sockets.Init();'):
                                 network_init.index('\tm_System->RegisterCommand(')]
    regions = {'allocate': allocate, 'clear': function(world, 'void World::ClearEntityCache()'),
               'release': release, 'network_reset': reset_address}
    text = '''// Generated from exact production source regions. Actual class declarations come from SDK headers.
#pragma once
using deltaCache_t = World::deltaCache_t;
using frameCache_t = World::frameCache_t;
#define free CseObjectResetFree
inline bool AllocateCaches(int maxCacheIndex, unsigned int frameBytes, frameCache_t*& frameCache,
    deltaCache_t*& deltaCache, ControlledSystem* m_System) {
'''+allocate+'''    return true;
}
inline void ClearCaches(int maxCacheIndex, frameCache_t* frameCache, deltaCache_t* deltaCache,
    int& cacheHits, int& cacheFaults)
'''+regions['clear']+'''
inline void ReleaseCaches(int maxCacheIndex, frameCache_t*& frameCache, deltaCache_t*& deltaCache) {
    (void)maxCacheIndex; // The original negative-control release did not consume this controlled binding.
'''+release+'''}
#undef free
inline void ResetAddress(NetAddress& m_LocalAddress) {
'''+reset_address+'''}
'''
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(text)
    receipt = {'source_root': str(source_root), 'production_sources': {
        str(path.relative_to(source_root)): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in [world_path, network_path]}, 'regions': {
        name: {'text': content, 'sha256': hashlib.sha256(content.encode()).hexdigest()}
        for name, content in regions.items()}, 'generated_sha256': hashlib.sha256(output.read_bytes()).hexdigest(),
        'binding': 'actual World nested cache records and NetAddress; controlled allocator/error scope; no World/Network instance or network Init run'}
    output.with_suffix('.json').write_text(json.dumps(receipt, indent=2))
    return receipt


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    extract(options.source_root, options.output)
