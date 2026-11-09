"""Bounded received-message evidence for the unchanged loopback server fixture.

This does not decode arbitrary entity/delta streams or establish rendered/audible
output. It locates strict fixture-specific protocol records inside actual demo
network frames, following the retained SDK's bit layouts and message registration.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import socket
import struct

from validate_demo import (HEADER, ENTRY, COMMAND, SEQUENCE, DEMO_INFO_BYTES,
                           SIMULATED_ORIGIN_OFFSET, HEALTH_OFFSET, MOVEMENT_OFFSET,
                           FIXED_PAYLOAD_BYTES, validate_demo)

SVC_SOUND = 6
SVC_TEMPENTITY = 23
SVC_NEWUSERMSG = 39
SVC_RESOURCELIST = 43
TE_TEXTMESSAGE = 29
CUSTOM_BODY = b'\x5a\xff\x7f\x78\x56\x34\x12CSE custom message boundary\0'
FIXTURE_SOUND = 'cse_fixture/probe.wav'
FIXTURE_RESOURCE = 'cse_fixture/probe.txt'
FIXTURE_MODEL = 'models/w_c4.mdl'


class Bits:
    def __init__(self, data):
        self.data = data
        self.position = 0

    def take(self, count):
        if count < 0 or self.position + count > len(self.data) * 8:
            raise ValueError('truncated bit record')
        result = 0
        for index in range(count):
            bit = self.position + index
            result |= ((self.data[bit // 8] >> (bit % 8)) & 1) << index
        self.position += count
        return result

    def string(self, limit=260):
        value = bytearray()
        for _ in range(limit):
            character = self.take(8)
            if not character:
                return value.decode('ascii')
            if not 32 <= character < 127:
                raise ValueError('non-ASCII resource name')
            value.append(character)
        raise ValueError('unterminated resource name')


def parse_resource_list(data):
    bits = Bits(data)
    count = bits.take(12)
    if not 3 <= count <= 4095:
        raise ValueError('invalid fixture resource count')
    resources = []
    for _ in range(count):
        kind = bits.take(4)
        if kind > 6:
            raise ValueError('invalid resource type')
        name = bits.string()
        index = bits.take(12)
        size = bits.take(24)
        flags = bits.take(3)
        if flags & 4:
            bits.take(16 * 8)
        if bits.take(1):
            bits.take(32 * 8)
        resources.append({'type': kind, 'name': name, 'index': index, 'bytes': size})
    required = bool(bits.take(1))
    selected = []
    previous = 0
    if required:
        while bits.take(1):
            index = previous + bits.take(5) if bits.take(1) else bits.take(10)
            if not previous <= index < len(resources):
                raise ValueError('invalid consistency resource index')
            selected.append(resources[index]['name'])
            previous = index
            if len(selected) > 1024:
                raise ValueError('excessive consistency list')
    names = {item['name']: item for item in resources}
    if not all(name in names for name in (FIXTURE_SOUND, FIXTURE_RESOURCE, FIXTURE_MODEL)):
        raise ValueError('not the fixture resource list')
    if not required or FIXTURE_MODEL not in selected:
        raise ValueError('fixture model consistency was not requested')
    if names[FIXTURE_SOUND]['type'] != 0 or names[FIXTURE_MODEL]['type'] != 2:
        raise ValueError('fixture resource types differ')
    return {'count': count, 'sound': names[FIXTURE_SOUND], 'resource': names[FIXTURE_RESOURCE],
            'model': names[FIXTURE_MODEL], 'consistency_required': required,
            'model_consistency_requested': True, 'encoded_bytes': (bits.position + 7) // 8}


def parse_sound(data):
    bits = Bits(data)
    flags = bits.take(9)
    volume = bits.take(8) / 255 if flags & 1 else 1.0
    attenuation = bits.take(8) / 64 if flags & 2 else 1.0
    channel = bits.take(3)
    entity = bits.take(11)
    index = bits.take(16 if flags & 4 else 8)
    axes = [bits.take(1) for _ in range(3)]
    origin = []
    for present in axes:
        value = 0.0
        if present:
            integer = bits.take(1)
            fraction = bits.take(1)
            if integer or fraction:
                negative = bits.take(1)
                value = (bits.take(12) if integer else 0) + (bits.take(3) / 8 if fraction else 0)
                if negative:
                    value = -value
        origin.append(value)
    pitch = bits.take(8) if flags & 8 else 100
    return {'flags': flags, 'volume': volume, 'attenuation': attenuation, 'channel': channel,
            'entity': entity, 'sound_index': index, 'origin': origin, 'pitch': pitch,
            'encoded_bytes': (bits.position + 7) // 8}


def demo_records(data, live=False):
    if len(data) < HEADER.size:
        return []
    stamp, protocol, network, _, _, _, directory = HEADER.unpack_from(data)
    if stamp[:6] != b'HLDEMO' or (protocol, network) != (5, 48):
        raise ValueError('unexpected demo header')
    ranges = [(HEADER.size, len(data))]
    if not live:
        if not HEADER.size <= directory <= len(data) - 4:
            raise ValueError('unfinished demo')
        count = struct.unpack_from('<i', data, directory)[0]
        ranges = []
        for index in range(count):
            entry = ENTRY.unpack_from(data, directory + 4 + index * ENTRY.size)
            ranges.append((entry[6], entry[6] + entry[7]))
    records = []
    for offset, end in ranges:
        while offset < end:
            begin = offset
            try:
                if offset + COMMAND.size > end:
                    raise ValueError('partial command')
                kind, time, frame = COMMAND.unpack_from(data, offset)
                offset += COMMAND.size
                if not math.isfinite(time):
                    raise ValueError('nonfinite frame time')
                if kind in (0, 1):
                    if offset + DEMO_INFO_BYTES + SEQUENCE.size > end:
                        raise ValueError('partial demo info')
                    info = data[offset:offset + DEMO_INFO_BYTES]
                    offset += DEMO_INFO_BYTES
                    sequence = SEQUENCE.unpack_from(data, offset)
                    offset += SEQUENCE.size
                    size = sequence[-1]
                    if not 0 <= size <= 65536 or offset + size > end:
                        raise ValueError('partial network message')
                    records.append({'kind': kind, 'time': time, 'frame': frame, 'offset': offset,
                                    'payload': data[offset:offset + size], 'sequence': sequence[0],
                                    'health': struct.unpack_from('<i', info, HEALTH_OFFSET)[0],
                                    'origin': struct.unpack_from('<3f', info, SIMULATED_ORIGIN_OFFSET),
                                    'movement': struct.unpack_from('<3f', info, MOVEMENT_OFFSET)})
                    offset += size
                elif kind in (2, 5):
                    pass
                elif kind in FIXED_PAYLOAD_BYTES:
                    offset += FIXED_PAYLOAD_BYTES[kind]
                elif kind == 8:
                    size = struct.unpack_from('<i', data, offset + 4)[0]
                    if size < 0:
                        raise ValueError('invalid sound record')
                    offset += 8 + size + 16
                elif kind == 9:
                    size = struct.unpack_from('<i', data, offset)[0]
                    if size < 0:
                        raise ValueError('invalid client record')
                    offset += 4 + size
                else:
                    raise ValueError('unknown demo command')
                if offset > end:
                    raise ValueError('partial record')
            except (ValueError, struct.error):
                if live:
                    return records
                raise ValueError(f'invalid recorded command at {begin}')
    return records


def live_state(data, movement=False):
    samples = [r for r in demo_records(data, live=True) if r['kind'] == 1 and r['payload'] and r['health'] > 0]
    if not samples:
        return False
    if not movement:
        return True
    span = [max(r['origin'][axis] for r in samples) - min(r['origin'][axis] for r in samples) for axis in range(3)]
    return len(samples) >= 100 and max(span) > 4 and any(any(value != 0 for value in r['movement']) for r in samples)


def _occurrences(data, marker):
    begin = 0
    while True:
        index = data.find(marker, begin)
        if index < 0:
            return
        yield index
        begin = index + 1


def message_registrations(records):
    result = {}
    for name in ('CSEProbe', 'SayText', 'TextMsg', 'ShowMenu'):
        suffix = name.encode().ljust(16, b'\0')
        matches = set()
        for record in records:
            for position in _occurrences(record['payload'], suffix):
                start = position - 3
                if start >= 0 and record['payload'][start] == SVC_NEWUSERMSG:
                    matches.add((record['payload'][start + 1], record['payload'][start + 2]))
        if len(matches) == 1:
            result[name] = next(iter(matches))
    return result


def variable_messages(records, name):
    registration = message_registrations(records).get(name)
    if not registration or registration[1] != 255:
        return []
    matches = []
    for record in records:
        if record['kind'] != 1:
            continue
        payload = record['payload']
        for position in _occurrences(payload, bytes([registration[0]])):
            if position + 2 <= len(payload):
                size = payload[position + 1]
                if position + 2 + size <= len(payload):
                    matches.append({'frame': record['frame'], 'sequence': record['sequence'],
                                    'body': payload[position + 2:position + 2 + size]})
    return matches


def text_deliveries(records, marker):
    # AMXX client_print(print_chat) sends TextMsg: HUD_PRINTTALK, "%s", argument.
    expected = b'\x03%s\0' + marker.encode('ascii') + b'\n\0'
    return [{key: value for key, value in message.items() if key != 'body'}
            for message in variable_messages(records, 'TextMsg') if message['body'] == expected]


def live_message_state(data, ack=False):
    records = demo_records(data, live=True)
    if ack:
        return bool(text_deliveries(records, 'CSE_FIXTURE_ACK_RECEIVED'))
    return bool(text_deliveries(records, 'CSE_FIXTURE_CHAT_READY')) and any(
        message['body'] == CUSTOM_BODY for message in variable_messages(records, 'CSEProbe'))


def analyze_messages(data, client_console, player_id):
    state = validate_demo(data, min_network_frames=100, min_duration=2.0,
                          require_alive=True, require_movement=True)
    records = demo_records(data)
    registrations = message_registrations(records)
    if 'CSEProbe' not in registrations or registrations['CSEProbe'][1] != 255:
        raise ValueError('missing unique variable CSEProbe registration')
    custom_id = registrations['CSEProbe'][0]
    custom = []
    resource_candidates = []
    hud = []
    for record in records:
        payload = record['payload']
        for offset in _occurrences(payload, bytes([custom_id, len(CUSTOM_BODY)]) + CUSTOM_BODY):
            if record['kind'] == 1:
                custom.append({'frame': record['frame'], 'sequence': record['sequence'], 'offset': record['offset'] + offset})
        if record['kind'] == 0:
            for offset in _occurrences(payload, bytes([SVC_RESOURCELIST])):
                try:
                    resource_candidates.append(parse_resource_list(payload[offset + 1:]))
                except (ValueError, UnicodeError):
                    pass
        for offset in _occurrences(payload, bytes([SVC_TEMPENTITY, TE_TEXTMESSAGE])):
            # TE_TEXTMESSAGE: channel,x,y,effect,RGBA pair,fade-in/out/hold,string.
            if offset + 22 <= len(payload):
                effect = payload[offset + 7]
                text_offset = offset + 22 + (2 if effect == 2 else 0)
                marker = b'CSE_FIXTURE_HUD_READY\0'
                if effect in (0, 1, 2) and payload[text_offset:text_offset + len(marker)] == marker:
                    hud.append({'frame': record['frame'], 'sequence': record['sequence'],
                                'color': list(payload[offset + 8:offset + 11])})
    if not custom or not hud or not resource_candidates:
        raise ValueError('missing received custom message, HUD or consistency resource list')
    resource = resource_candidates[0]
    if any(item != resource for item in resource_candidates):
        raise ValueError('ambiguous fixture resource list')
    sound = []
    for record in records:
        if record['kind'] != 1:
            continue
        for offset in _occurrences(record['payload'], bytes([SVC_SOUND])):
            try:
                item = parse_sound(record['payload'][offset + 1:])
                if item['sound_index'] == resource['sound']['index'] and item['entity'] == player_id \
                        and item['channel'] == 3 and item['pitch'] == 100 and item['volume'] == 1 \
                        and not item['flags'] & (16 | 32):
                    item.update({'frame': record['frame'], 'sequence': record['sequence']})
                    sound.append(item)
            except ValueError:
                pass
    if not sound:
        raise ValueError('missing received fixture sound indexed by resource list')
    if 'UserMsg: No pfn CSEProbe' not in client_console:
        raise ValueError('client did not consume/skip the unhooked custom message')
    received = {}
    for marker in ('CSE_FIXTURE_CHAT_READY', 'CSE_FIXTURE_ACK_RECEIVED'):
        received[marker] = text_deliveries(records, marker)
        if not received[marker]:
            raise ValueError('missing received registered TextMsg body: ' + marker)
    menu = []
    for message in variable_messages(records, 'ShowMenu'):
        body = message['body']
        if len(body) >= 5 and struct.unpack_from('<H', body)[0] & 1 and body[3] == 0 \
                and b'\\yCSE_FIXTURE_MENU_READY\n\\w\n' in body[4:]:
            menu.append({key: value for key, value in message.items() if key != 'body'})
    if not menu:
        raise ValueError('missing received registered ShowMenu fixture body')
    received['CSE_FIXTURE_MENU_READY'] = menu
    return {'recorded_state': state, 'registrations': registrations, 'custom_message': custom,
            'hud_delivery': hud, 'resource_list': resource, 'sound_delivery': sound,
            'received_markers': received, 'scope': 'received protocol bytes and unhooked custom skip; no voice/audibility claim'}


def query_info(port):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp:
        udp.settimeout(3)
        address = ('127.0.0.1', port)
        request = b'\xff' * 4 + b'TSource Engine Query\0'
        udp.sendto(request, address)
        data, sender = udp.recvfrom(65536)
        if sender != address:
            raise ValueError('unexpected query endpoint')
        if data[:5] == b'\xff\xff\xff\xffA':
            if len(data) != 9:
                raise ValueError('invalid challenge')
            udp.sendto(request + data[5:], address)
            data, sender = udp.recvfrom(65536)
        if sender != address or data[:5] != b'\xff\xff\xff\xffI':
            raise ValueError('unexpected A2S_INFO response')
        position = 6
        strings = []
        for _ in range(4):
            end = data.index(b'\0', position)
            strings.append(data[position:end].decode('ascii', errors='replace'))
            position = end + 1
        app_id = struct.unpack_from('<H', data, position)[0]
        players, maximum, bots, server_type, environment, visibility, vac = struct.unpack_from('<7B', data, position + 2)
        # Discard hostname, server ID, keywords and all unrequested metadata.
        return {'query_available': True, 'address': '127.0.0.1', 'port': port, 'map': strings[1], 'folder': strings[2],
                'app_id': app_id, 'players': players, 'maximum_players': maximum,
                'vac_secure': vac == 1, 'password_required': visibility != 0}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--demo', type=Path)
    parser.add_argument('--console', type=Path)
    parser.add_argument('--player-id', type=int)
    parser.add_argument('--live', choices=('alive', 'movement', 'messages', 'ack'))
    parser.add_argument('--query-port', type=int)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.query_port:
        try:
            result = query_info(args.query_port)
        except (TimeoutError, ConnectionResetError) as error:
            result = {'query_available': False, 'error': type(error).__name__,
                      'address': '127.0.0.1', 'port': args.query_port}
    elif args.live:
        data = args.demo.read_bytes()
        if args.live in ('messages', 'ack'):
            return 0 if live_message_state(data, args.live == 'ack') else 2
        return 0 if live_state(data, args.live == 'movement') else 2
    else:
        result = analyze_messages(args.demo.read_bytes(), args.console.read_text(errors='replace'), args.player_id)
    encoded = json.dumps(result, indent=2) + '\n'
    if args.output:
        args.output.write_text(encoded)
    else:
        print(encoded, end='')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
