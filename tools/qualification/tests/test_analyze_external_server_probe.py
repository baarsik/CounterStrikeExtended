"""Strict bounded protocol-decoder tests; live client evidence is a separate gate."""
import importlib.util
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
spec = importlib.util.spec_from_file_location('external_probe', ROOT / 'analyze_external_server_probe.py')
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


class Writer:
    def __init__(self):
        self.bits = []

    def put(self, value, width):
        self.bits.extend((value >> shift) & 1 for shift in range(width))
        return self

    def text(self, value):
        for character in value.encode('ascii') + b'\0':
            self.put(character, 8)
        return self

    def data(self):
        result = bytearray((len(self.bits) + 7) // 8)
        for index, value in enumerate(self.bits):
            result[index // 8] |= value << (index % 8)
        return bytes(result)


def resources(consistency=True, model_selected=True):
    writer = Writer().put(3, 12)
    for kind, name, index, size in (
        (0, probe.FIXTURE_SOUND, 1, 4454),
        (4, probe.FIXTURE_RESOURCE, 1, 44),
        (2, probe.FIXTURE_MODEL, 2, 23992),
    ):
        writer.put(kind, 4).text(name).put(index, 12).put(size, 24).put(0, 3).put(0, 1)
    writer.put(int(consistency), 1)
    if consistency:
        writer.put(1, 1).put(1, 1).put(2 if model_selected else 0, 5).put(0, 1)
    return writer.data()


def info_response(vac):
    return (b'\xff' * 4 + b'I\x30owned\0de_dust2\0cstrike\0Counter-Strike\0'
            + struct.pack('<H7B', 10, 1, 8, 0, ord('d'), ord('w'), 0, vac))


class ProbeTests(unittest.TestCase):
    def test_fixture_resources_and_consistency(self):
        result = probe.parse_resource_list(resources())
        self.assertTrue(result['model_consistency_requested'])
        self.assertEqual(result['model']['bytes'], 23992)
        self.assertEqual(result['resource']['type'], 4)
        self.assertEqual(result['sound']['index'], 1)

    def test_missing_consistency_rejected(self):
        for data in (resources(False), resources(model_selected=False)):
            with self.subTest(data=data), self.assertRaisesRegex(ValueError, 'consistency'):
                probe.parse_resource_list(data)

    def test_resource_truncation_rejected(self):
        data = resources()
        for length in range(len(data)):
            with self.subTest(length=length), self.assertRaises(ValueError):
                probe.parse_resource_list(data[:length])

    def test_default_sound_fields(self):
        data = Writer().put(0, 9).put(3, 3).put(7, 11).put(1, 8).put(0, 3).data()
        result = probe.parse_sound(data)
        self.assertEqual(result['entity'], 7)
        self.assertEqual(result['channel'], 3)
        self.assertEqual(result['volume'], 1)
        self.assertEqual(result['origin'], [0, 0, 0])
        self.assertEqual(result['pitch'], 100)

    def test_optional_sound_fields_and_coordinates(self):
        writer = Writer().put(15, 9).put(128, 8).put(32, 8).put(3, 3).put(7, 11).put(300, 16)
        writer.put(1, 1).put(1, 1).put(0, 1)
        writer.put(1, 1).put(1, 1).put(1, 1).put(12, 12).put(4, 3)
        writer.put(0, 1).put(1, 1).put(0, 1).put(2, 3).put(180, 8)
        result = probe.parse_sound(writer.data())
        self.assertEqual(result['origin'], [-12.5, 0.25, 0])
        self.assertEqual(result['sound_index'], 300)
        self.assertEqual(result['pitch'], 180)
        self.assertEqual(result['attenuation'], 0.5)
        self.assertEqual(result['volume'], 128 / 255)
        with self.assertRaises(ValueError):
            probe.parse_sound(writer.data()[:-1])

    def test_registered_text_envelope_requires_exact_destination_format_and_length(self):
        marker = 'CSE_FIXTURE_CHAT_READY'
        body = b'\x03%s\0' + marker.encode() + b'\n\0'
        registration = bytes([39, 77, 255]) + b'TextMsg'.ljust(16, b'\0')
        records = [{'kind': 0, 'payload': registration, 'frame': 1, 'sequence': 1},
                   {'kind': 1, 'payload': bytes([77, len(body)]) + body, 'frame': 2, 'sequence': 2}]
        self.assertEqual(probe.text_deliveries(records, marker), [{'frame': 2, 'sequence': 2}])
        for mutant in (bytes([77, len(body)]) + b'\x02' + body[1:],
                       bytes([77, len(body) - 1]) + body,
                       bytes([76, len(body)]) + body):
            records[1]['payload'] = mutant
            self.assertEqual(probe.text_deliveries(records, marker), [])

    def test_ambiguous_registration_rejects_text_signature(self):
        marker = 'CSE_FIXTURE_ACK_RECEIVED'
        body = b'\x03%s\0' + marker.encode() + b'\n\0'
        records = [{'kind': 0, 'payload': b''.join(bytes([39, index, 255])
                    + b'TextMsg'.ljust(16, b'\0') for index in (77, 78)), 'frame': 1, 'sequence': 1},
                   {'kind': 1, 'payload': bytes([77, len(body)]) + body, 'frame': 2, 'sequence': 2}]
        self.assertEqual(probe.text_deliveries(records, marker), [])

    def test_query_challenge_and_secure_fields(self):
        with patch.object(probe.socket, 'socket') as factory:
            udp = factory.return_value.__enter__.return_value
            udp.recvfrom.side_effect = [(b'\xff' * 4 + b'A1234', ('127.0.0.1', 27438)),
                                        (info_response(1), ('127.0.0.1', 27438))]
            result = probe.query_info(27438)
            self.assertTrue(result['vac_secure'])
            self.assertEqual(result['folder'], 'cstrike')
            self.assertNotIn('hostname', result)
            self.assertEqual(udp.sendto.call_args.args[0][-4:], b'1234')

    def test_query_insecure_and_foreign_sender_not_hidden(self):
        with patch.object(probe.socket, 'socket') as factory:
            udp = factory.return_value.__enter__.return_value
            udp.recvfrom.return_value = (info_response(0), ('127.0.0.1', 27438))
            self.assertFalse(probe.query_info(27438)['vac_secure'])
            udp.recvfrom.return_value = (info_response(1), ('127.0.0.1', 27439))
            with self.assertRaisesRegex(ValueError, 'endpoint'):
                probe.query_info(27438)

    def test_live_demo_complete_prefix_and_partial_tail(self):
        header = probe.HEADER.pack(b'HLDEMO\0\0', 5, 48, b'de_dust2', b'cstrike', 0, 0)
        info = bytearray(probe.DEMO_INFO_BYTES)
        struct.pack_into('<i', info, probe.HEALTH_OFFSET, 100)
        sequence = probe.SEQUENCE.pack(1, 0, 0, 0, 0, 0, 0, 1)
        record = probe.COMMAND.pack(1, 1.0, 1) + info + sequence + b'\x01'
        data = header + record + record[:-1]
        self.assertEqual(len(probe.demo_records(data, live=True)), 1)
        self.assertTrue(probe.live_state(data))
        self.assertFalse(probe.live_state(data, movement=True))
        with self.assertRaisesRegex(ValueError, 'unfinished'):
            probe.demo_records(data)


if __name__ == '__main__':
    unittest.main()
