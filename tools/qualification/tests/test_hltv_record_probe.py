"""HLTV-profile structural controls; live producers/readers remain separate gates."""
import hashlib
import importlib.util
import math
import os
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, os.environ.get('CSE_HLTV_DECODER_DIRECTORY', str(ROOT)))
import analyze_external_server_probe as decoder
spec = importlib.util.spec_from_file_location('hltv_record_probe', ROOT / 'hltv_record_probe.py')
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)
MARKER = 'CSE_HLTV_BROADCAST_TEST'


def demo(marker=MARKER, wide=False, initial=0.0):
    info = bytes(decoder.DEMO_INFO_BYTES + (8 if wide else 0))
    message = bytes([0]) + struct.pack('<I6f', 0x00FFA000, -1, -1, 0.5, 2, 5, 0)
    message += marker.encode('ascii') + b'\0'
    payload = bytes([51, len(message) + 1, 6]) + message
    def network(kind, time, frame, message):
        return decoder.COMMAND.pack(kind, time, frame) + info + decoder.SEQUENCE.pack(
            frame, frame, frame, frame, frame, frame, frame, len(message)) + message
    startup = network(0, initial, 0, b'\x01') + decoder.COMMAND.pack(5, initial, 0)
    normal = decoder.COMMAND.pack(2, initial, 0)
    for index in range(1, 101):
        time = initial if index == 1 else index * 0.05
        normal += network(1, time, index, payload if index == 20 else b'\x01')
        if index != 100:
            normal += decoder.COMMAND.pack(4, time, index) + bytes(32)
    normal += decoder.COMMAND.pack(5, 5, 100)
    offset = decoder.HEADER.size
    directory = offset + len(startup) + len(normal)
    header = decoder.HEADER.pack(b'HLDEMO\0\0', 5, 48, b'de_dust2', b'cstrike', 0, directory)
    entries = decoder.ENTRY.pack(0, b'LOADING', 0, 0, 0, 0, offset, len(startup))
    entries += decoder.ENTRY.pack(1, b'Playback', 0, 0, 5, 100, offset + len(startup), len(normal))
    return header + startup + normal + struct.pack('<i', 2) + entries


def clock_evidence(data):
    return {'filename': 'control.dem', 'demo_sha256': hashlib.sha256(data).hexdigest(),
            'console': 'Start recording to control.dem.\n--- HLTV Status ---\n'
                       'Online 00:02, FPS 50, Version 1 (Win32)\n'
                       'Recording to control.dem, Length 0.1 sec.\nCompleted demo control.dem.\n'}


def command_offsets(data):
    directory = decoder.HEADER.unpack_from(data)[-1]
    result = []
    for index in range(2):
        entry = decoder.ENTRY.unpack_from(data, directory + 4 + index * decoder.ENTRY.size)
        offset = entry[6]
        while offset < entry[6] + entry[7]:
            begin = offset
            kind, time, frame = decoder.COMMAND.unpack_from(data, offset)
            offset += decoder.COMMAND.size
            if kind in (0, 1):
                offset += decoder.DEMO_INFO_BYTES
                sequence = decoder.SEQUENCE.unpack_from(data, offset)
                offset += decoder.SEQUENCE.size + sequence[-1]
            elif kind == 4:
                offset += 32
            result.append((begin, kind, frame))
    return result


class HltvControls(unittest.TestCase):
    def test_zero_viewer_health_is_valid_broadcast_profile(self):
        result = probe.validate_broadcast(demo(), MARKER, decoder, clock_evidence(demo()))
        self.assertIsInstance(result, dict)
        self.assertEqual(result['recorded_state']['nonempty_network_frames'], 100)
        self.assertEqual(result['recorded_state']['alive_samples'], 0)
        self.assertEqual(result['full_eof_trailing_bytes'], 0)
        self.assertEqual(len(result['marker_records']), 1)

    def test_wrong_marker_or_nonterminated_match_rejected(self):
        for marker in ['OTHER_MARKER', MARKER + '_WRONG']:
            with self.subTest(marker=marker):
                with self.assertRaises(ValueError):
                    probe.validate_broadcast(demo(marker), MARKER, decoder, clock_evidence(demo(marker)))

    def test_truncated_trailing_and_native_width_info_rejected(self):
        original = demo()
        for data in [original[:-1], original + b'\0', demo(wide=True)]:
            with self.subTest(length=len(data)):
                with self.assertRaises(ValueError):
                    probe.validate_broadcast(data, MARKER, decoder, clock_evidence(data))

    def test_missing_stop_rejected(self):
        data = bytearray(demo())
        directory = decoder.HEADER.unpack_from(data)[-1]
        data[directory - decoder.COMMAND.size] = 2
        with self.assertRaises(ValueError):
            probe.validate_broadcast(data, MARKER, decoder, clock_evidence(data))

    def test_valid_initial_roundup_and_rounddown_bits_preserved(self):
        for initial in [-2.0 ** -25, 2.0 ** -25]:
            with self.subTest(initial=initial):
                data = demo(initial=initial)
                before = hashlib.sha256(data).hexdigest()
                try:
                    result = probe.validate_broadcast(data, MARKER, decoder, clock_evidence(data))
                except ValueError as error:
                    self.fail('Valid same-tick binary32 rounding profile rejected: ' + str(error))
                self.assertIn('initial_tick', result)
                self.assertEqual(result['initial_tick']['bits'], f"{struct.unpack('<I', struct.pack('<f', initial))[0]:08x}")
                self.assertEqual(hashlib.sha256(data).hexdigest(), before)

    def test_clock_bound_is_derived_from_trusted_post_start_status(self):
        evidence = clock_evidence(demo())
        result = probe.trusted_clock_bound(evidence['console'], evidence['filename'])
        self.assertIsInstance(result, dict)
        self.assertEqual(result['upper_seconds'], 3)
        self.assertEqual(result['bound_seconds'], 2.0 ** -23)

    def test_missing_malformed_or_contradictory_clock_receipts_rejected(self):
        data = demo()
        original = clock_evidence(data)
        invalid = [None, {}, dict(original, demo_sha256='0' * 64), dict(original, filename='other.dem'),
                   dict(original, upper_seconds=1), dict(original, console=original['console'].replace('00:02', '00:99')),
                   dict(original, console=original['console'].replace('00:02', '10:00')),
                   dict(original, console='Online 00:02\nStart recording to control.dem.\nCompleted demo control.dem.\n')]
        for evidence in invalid:
            with self.subTest(evidence=evidence):
                with self.assertRaises(ValueError):
                    probe.validate_broadcast(data, MARKER, decoder, evidence)

    def test_negative_after_positive_unmatched_and_deep_negative_rejected(self):
        original = demo(initial=-2.0 ** -25)
        offsets = command_offsets(original)
        bad = []
        later = bytearray(original)
        target = next(offset for offset, kind, frame in offsets if kind == 1 and frame == 20)
        struct.pack_into('<f', later, target + 1, -2.0 ** -25)
        bad.append(later)
        unmatched = bytearray(original)
        struct.pack_into('<f', unmatched, offsets[1][0] + 1, -2.0 ** -26)
        bad.append(unmatched)
        bad.append(demo(initial=-2.0 ** -22))
        for data in bad:
            with self.subTest(data_sha=hashlib.sha256(data).hexdigest()):
                with self.assertRaises(ValueError):
                    probe.validate_broadcast(data, MARKER, decoder, clock_evidence(data))

    def test_initial_order_or_frame_identity_mismatch_rejected(self):
        original = demo()
        offsets = command_offsets(original)
        for index, replacement in [(2, 3), (3, 7)]:
            data = bytearray(original)
            struct.pack_into('<i', data, offsets[index][0] + 5, replacement)
            with self.subTest(index=index):
                with self.assertRaises(ValueError):
                    probe.validate_broadcast(data, MARKER, decoder, clock_evidence(data))

    def test_nonfinite_initial_later_and_directory_time_rejected(self):
        original = demo()
        offsets = command_offsets(original)
        directory = decoder.HEADER.unpack_from(original)[-1]
        for offset, value in [(offsets[0][0] + 1, math.nan), (offsets[10][0] + 1, math.inf),
                              (directory + 4 + decoder.ENTRY.size + 76, math.nan)]:
            data = bytearray(original)
            struct.pack_into('<f', data, offset, value)
            with self.subTest(offset=offset):
                with self.assertRaises(ValueError):
                    probe.validate_broadcast(data, MARKER, decoder, clock_evidence(data))

    def test_derived_bound_boundary_and_next_binary32_value(self):
        bound = 2.0 ** -23
        data = demo(initial=-bound)
        result = probe.validate_broadcast(data, MARKER, decoder, clock_evidence(data))
        self.assertEqual(abs(result['initial_tick']['time']), bound)
        bits = struct.unpack('<I', struct.pack('<f', -bound))[0]
        deeper = struct.unpack('<f', struct.pack('<I', bits + 1))[0]
        data = demo(initial=deeper)
        with self.assertRaises(ValueError):
            probe.validate_broadcast(data, MARKER, decoder, clock_evidence(data))

    def test_nonfinite_client_data_rejected(self):
        data = bytearray(demo())
        offset = next(offset for offset, kind, frame in command_offsets(data) if kind == 4 and frame == 20)
        struct.pack_into('<f', data, offset + decoder.COMMAND.size, math.nan)
        with self.assertRaises(ValueError):
            probe.validate_broadcast(data, MARKER, decoder, clock_evidence(data))

    def test_director_marker_length_type_and_finite_fields_checked(self):
        original = demo()
        marker_offset = original.index(MARKER.encode('ascii'))
        command = marker_offset - 32
        bad = []
        for offset, value in [(command + 1, 1), (command + 1, 255), (command + 2, 7), (command + 3, 255)]:
            data = bytearray(original)
            data[offset] = value
            bad.append(data)
        data = bytearray(original)
        struct.pack_into('<f', data, command + 8, math.nan)
        bad.append(data)
        for data in bad:
            with self.subTest(data_sha=hashlib.sha256(data).hexdigest()):
                with self.assertRaises(ValueError):
                    probe.validate_broadcast(data, MARKER, decoder, clock_evidence(data))

    def test_input_identity_checked_without_mutation(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'input.dem'
            data = demo()
            path.write_bytes(data)
            expected = hashlib.sha256(data).hexdigest()
            self.assertEqual(probe.verify_input(path, expected), expected)
            with self.assertRaises(ValueError):
                probe.verify_input(path, '0' * 64)
            self.assertEqual(path.read_bytes(), data)

    def test_reader_requires_ordered_actual_eof(self):
        start = 'CSE_HLTV_REPLAY_BEGIN\nPlaying demo from cse_hltv_broadcast.dem\n'
        end = 'Host_EndGame: End of demo.\nCSE_HLTV_REPLAY_DONE\n'
        self.assertTrue(probe.reader_completed(start + end))
        for text in [start, end, start + 'CSE_HLTV_REPLAY_DONE\n', end + start]:
            with self.subTest(console=text):
                self.assertFalse(probe.reader_completed(text))


if __name__ == '__main__':
    unittest.main()
