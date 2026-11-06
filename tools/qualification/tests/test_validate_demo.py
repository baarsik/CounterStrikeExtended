"""Exercise the acceptance parser with independently constructed x86 demo data."""

import contextlib
import io
import json
import math
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from validate_demo import main, validate_demo


def command(kind, time, frame, payload=b""):
    return struct.pack("<Bfi", kind, time, frame) + payload


def network(time, frame, sequence, x=0.0, health=100, forward=0.0, payload=b"\x01"):
    # Hand-derived Win32 demo_info_t fields, independent of the parser's layouts.
    info = bytearray(436)
    struct.pack_into("<3f", info, 104, x, 0.0, 0.0)  # rp.simorg
    struct.pack_into("<i", info, 144, health)  # rp.health
    struct.pack_into("<f", info, 252, forward)  # cmd.forwardmove
    sequence_info = struct.pack("<8i", sequence, 0, 0, 0, sequence, 0, 0, len(payload))
    return command(1, time, frame, info + sequence_info + payload)


def fixture(normal=None, frame_count=2, duration=1.0):
    startup = command(0, 0.0, 0, bytes(436) + struct.pack("<8i", 0, 0, 0, 0, 0, 0, 0, 1) + b"\x01")
    startup += command(5, 0.0, 0)
    if normal is None:
        normal = command(2, 0.0, 0)
        normal += network(0.0, 0, 1)
        normal += network(1.0, 100, 2, x=24.0, forward=200.0)
        normal += command(5, 1.0, 100)
    directory_offset = 544 + len(startup) + len(normal)
    header = struct.pack("<8sii260s260sIi", b"HLDEMO\0\0", 5, 48, b"de_dust2", b"cstrike", 123,
                         directory_offset)
    directory = struct.pack("<i", 2)
    directory += struct.pack("<i64siifiii", 0, b"LOADING", 0, -1, 0.0, 0, 544, len(startup))
    directory += struct.pack("<i64siifiii", 1, b"Playback", 0, -1, duration, frame_count,
                             544 + len(startup), len(normal))
    return header + startup + normal + directory


class ValidateDemoTests(unittest.TestCase):
    def test_accepts_recording_with_live_player_and_movement(self):
        result = validate_demo(fixture(), min_network_frames=2, min_duration=1.0,
                               require_alive=True, require_movement=True)
        self.assertEqual(result.get("map"), "de_dust2")
        self.assertEqual(result.get("network_frames"), 2)
        self.assertEqual(result.get("alive_samples"), 2)
        self.assertEqual(result.get("duration_seconds"), 1.0)
        self.assertEqual(result.get("simulated_origin_span"), [24.0, 0.0, 0.0])

    def test_rejects_truncated_header(self):
        with self.assertRaisesRegex(ValueError, "header"):
            validate_demo(fixture()[:543])

    def test_rejects_missing_directory_from_unfinished_recording(self):
        data = bytearray(fixture())
        struct.pack_into("<i", data, 540, 0)
        with self.assertRaisesRegex(ValueError, "directory"):
            validate_demo(data)

    def test_rejects_wrong_protocol(self):
        data = bytearray(fixture())
        struct.pack_into("<i", data, 12, 47)
        with self.assertRaisesRegex(ValueError, "protocol"):
            validate_demo(data)

    def test_rejects_zero_directory_entries(self):
        data = bytearray(fixture())
        offset = struct.unpack_from("<i", data, 540)[0]
        struct.pack_into("<i", data, offset, 0)
        with self.assertRaisesRegex(ValueError, "entry count"):
            validate_demo(data)

    def test_rejects_truncated_directory(self):
        with self.assertRaisesRegex(ValueError, "directory"):
            validate_demo(fixture()[:-1])

    def test_rejects_overlapping_track_extents(self):
        data = bytearray(fixture())
        offset = struct.unpack_from("<i", data, 540)[0]
        struct.pack_into("<i", data, offset + 4 + 92 + 84, 544)
        with self.assertRaisesRegex(ValueError, "overlap"):
            validate_demo(data)

    def test_rejects_empty_gameplay_even_with_completed_header(self):
        data = fixture(command(2, 0.0, 0) + command(5, 1.0, 100), frame_count=0)
        with self.assertRaisesRegex(ValueError, "network frames"):
            validate_demo(data)

    def test_rejects_directory_frame_count_disagreeing_with_track(self):
        with self.assertRaisesRegex(ValueError, "frame count"):
            validate_demo(fixture(frame_count=3))

    def test_rejects_network_length_crossing_track_boundary(self):
        truncated = network(0.0, 0, 1)[:-1]
        with self.assertRaisesRegex(ValueError, "truncated"):
            validate_demo(fixture(truncated, frame_count=1))

    def test_rejects_negative_network_length(self):
        data = bytearray(network(0.0, 0, 1))
        struct.pack_into("<i", data, 9 + 436 + 28, -1)
        with self.assertRaisesRegex(ValueError, "message length"):
            validate_demo(fixture(data + command(5, 1.0, 1), frame_count=1))

    def test_rejects_unknown_command_and_missing_stop(self):
        for normal in (command(10, 0.0, 0), network(0.0, 0, 1)):
            with self.subTest(normal=normal[:1]):
                with self.assertRaises(ValueError):
                    validate_demo(fixture(normal, frame_count=1))

    def test_rejects_nonfinite_frame_time(self):
        with self.assertRaisesRegex(ValueError, "time"):
            validate_demo(fixture(network(math.nan, 0, 1) + command(5, 1.0, 1), frame_count=1))

    def test_enforces_duration_and_frame_requirements(self):
        for options in ({"min_network_frames": 3}, {"min_duration": 2.0}):
            with self.subTest(options=options):
                with self.assertRaises(ValueError):
                    validate_demo(fixture(), **options)

    def test_does_not_count_empty_messages_towards_frame_requirement(self):
        normal = network(0.0, 0, 1, payload=b"") + network(1.0, 100, 2)
        normal += command(5, 1.0, 100)
        with self.assertRaisesRegex(ValueError, "network frames"):
            validate_demo(fixture(normal), min_network_frames=2)

    def test_duration_gate_uses_network_samples_instead_of_only_stop_time(self):
        normal = network(0.0, 0, 1) + network(0.0, 0, 2) + command(5, 10.0, 1000)
        with self.assertRaisesRegex(ValueError, "duration"):
            validate_demo(fixture(normal, duration=10.0), min_duration=5.0)

    def test_cli_reports_accepted_asset_and_rejects_bad_input(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "recorded.dem"
            path.write_bytes(fixture())
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(main([str(path), "--require-movement"]), 0)
            self.assertEqual(json.loads(output.getvalue())["network_frames"], 2)
            path.write_bytes(b"unfinished")
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                main([str(path)])
            self.assertEqual(error.exception.code, 1)

    def test_rejects_dead_player_when_live_player_is_required(self):
        normal = network(0.0, 0, 1, health=0) + command(5, 1.0, 100)
        with self.assertRaisesRegex(ValueError, "alive"):
            validate_demo(fixture(normal, frame_count=1), require_alive=True)

    def test_requires_both_movement_command_and_origin_change(self):
        for forward, x in ((200.0, 0.0), (0.0, 24.0)):
            normal = network(0.0, 0, 1) + network(1.0, 100, 2, x=x, forward=forward)
            normal += command(5, 1.0, 100)
            with self.subTest(forward=forward, x=x):
                with self.assertRaisesRegex(ValueError, "movement"):
                    validate_demo(fixture(normal), require_movement=True)

    def test_skips_all_actual_auxiliary_frame_payloads(self):
        auxiliary = command(3, 0.0, 0, bytes(64))
        auxiliary += command(4, 0.0, 0, bytes(32))
        auxiliary += command(6, 0.0, 0, bytes(12 + 72))
        auxiliary += command(7, 0.0, 0, bytes(8))
        auxiliary += command(8, 0.0, 0, struct.pack("<ii", 1, 3) + b"wav" + bytes(16))
        auxiliary += command(9, 0.0, 0, struct.pack("<i", 2) + b"hi")
        normal = auxiliary + network(0.0, 0, 1) + command(5, 1.0, 100)
        self.assertEqual(validate_demo(fixture(normal, frame_count=1)).get("network_frames"), 1)


if __name__ == "__main__":
    unittest.main()
