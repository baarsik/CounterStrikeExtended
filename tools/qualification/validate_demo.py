"""Validate recorded Windows x86 GoldSrc demo assets for qualification.

Layouts come from engine/cl_demo.h and the writers in engine/cl_demo.cpp.
This is an asset/recorded-state check. It does not decode svc messages or prove
successful engine playback, completed sign-on, team selection, or input delivery.
"""

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path


HEADER = struct.Struct("<8sii260s260sIi")
ENTRY = struct.Struct("<i64siifiii")
COMMAND = struct.Struct("<Bfi")
INT = struct.Struct("<i")
SEQUENCE = struct.Struct("<8i")
MAX_ENTRIES = 1024  # MAX_DEMO_ENTRIES in cl_demo.cpp.
MAX_MESSAGE_BYTES = 65536  # MAX_POSSIBLE_MSG in cl_demo.cpp.

# Win32 natural alignment of the owning types: timestamp(4), ref_params_t(232),
# usercmd_t(52), movevars_t(132), view(12), viewmodel(4). Pointers on disk are x86.
DEMO_INFO_BYTES = 436
SIMULATED_ORIGIN_OFFSET = 104  # demo_info_t.rp.simorg
HEALTH_OFFSET = 144  # demo_info_t.rp.health
MOVEMENT_OFFSET = 252  # demo_info_t.cmd.forwardmove, sidemove, upmove
STARTUP = 0
NORMAL = 1
JUMP_TIME = 2
STOP = 5
FIXED_PAYLOAD_BYTES = {3: 64, 4: 32, 6: 12 + 72, 7: 8}


def _text(value):
    return value.split(b"\0", 1)[0].decode("utf-8", errors="replace")


class _TrackReader:
    def __init__(self, data, offset, end):
        self.data = data
        self.offset = offset
        self.end = end

    def take(self, count):
        if count < 0 or count > self.end - self.offset:
            raise ValueError(f"truncated frame payload at byte {self.offset}")
        begin = self.offset
        self.offset += count
        return memoryview(self.data)[begin:self.offset]

    def unpack(self, layout):
        return layout.unpack(self.take(layout.size))


def _read_track(data, entry):
    kind, description, flags, cd_track, duration, frames, offset, length = entry
    reader = _TrackReader(data, offset, offset + length)
    network_frames = 0
    samples = []
    network_times = []
    last_time = 0.0
    stopped = False
    while reader.offset < reader.end:
        command, time, frame = reader.unpack(COMMAND)
        if not math.isfinite(time):
            raise ValueError("nonfinite frame time")
        if kind == NORMAL and time < 0.0:
            raise ValueError("negative normal playback time")
        last_time = max(last_time, time)
        if command in (STARTUP, NORMAL):
            info = reader.take(DEMO_INFO_BYTES)
            sequence = reader.unpack(SEQUENCE)
            message_length = sequence[-1]
            if not 0 <= message_length <= MAX_MESSAGE_BYTES:
                raise ValueError("invalid network message length")
            reader.take(message_length)
            if command == NORMAL:
                network_frames += 1
                origin = struct.unpack_from("<3f", info, SIMULATED_ORIGIN_OFFSET)
                health = struct.unpack_from("<i", info, HEALTH_OFFSET)[0]
                movement = struct.unpack_from("<3f", info, MOVEMENT_OFFSET)
                if not all(math.isfinite(value) for value in (*origin, *movement)):
                    raise ValueError("nonfinite recorded player state")
                if message_length:
                    samples.append((health, origin, movement, sequence[0]))
                    network_times.append(time)
        elif command == STOP:
            if reader.offset != reader.end:
                raise ValueError("bytes after track STOP")
            stopped = True
        elif command == JUMP_TIME:
            pass
        elif command in FIXED_PAYLOAD_BYTES:
            reader.take(FIXED_PAYLOAD_BYTES[command])
        elif command == 8:  # demo_sound1_t, sample bytes, demo_sound2_t.
            channel, sample_bytes = struct.unpack("<ii", reader.take(8))
            if sample_bytes < 0:
                raise ValueError("negative sound sample length")
            reader.take(sample_bytes)
            reader.take(16)
        elif command == 9:  # CL_WriteClientDLLMessage.
            message_length = reader.unpack(INT)[0]
            reader.take(message_length)
        else:
            raise ValueError(f"unknown demo command {command}")
    if not stopped:
        raise ValueError("track is missing STOP")
    if kind == NORMAL and network_frames != frames:
        raise ValueError("directory frame count differs from recorded network frame count")
    if kind == NORMAL and not math.isclose(duration, last_time, abs_tol=0.001):
        raise ValueError("directory duration differs from recorded frame time")
    network_duration = max(network_times) - min(network_times) if network_times else 0.0
    return samples, network_frames, network_duration


def validate_demo(data, *, min_network_frames=1, min_duration=0.0,
                  require_alive=False, require_movement=False):
    """Return a JSON-compatible asset summary; raise ValueError on failed checks."""
    if min_network_frames < 1 or not math.isfinite(min_duration) or min_duration < 0:
        raise ValueError("requirements must use positive frame count and finite nonnegative duration")
    if len(data) < HEADER.size:
        raise ValueError("truncated demo header")
    stamp, demo_protocol, net_protocol, map_name, game, map_crc, directory_offset = HEADER.unpack_from(data)
    if stamp[:6] != b"HLDEMO":
        raise ValueError("invalid demo header stamp")
    if (demo_protocol, net_protocol) != (5, 48):
        raise ValueError("expected demo/network protocol 5/48")
    if not HEADER.size <= directory_offset <= len(data) - INT.size:
        raise ValueError("invalid or unfinished demo directory offset")
    entry_count = INT.unpack_from(data, directory_offset)[0]
    if not 1 <= entry_count <= MAX_ENTRIES:
        raise ValueError("invalid directory entry count")
    directory_end = directory_offset + INT.size + entry_count * ENTRY.size
    if directory_end != len(data):
        raise ValueError("truncated directory or unexpected trailing directory bytes")
    entries = []
    for index in range(entry_count):
        entry = ENTRY.unpack_from(data, directory_offset + INT.size + index * ENTRY.size)
        kind, description, flags, cd_track, duration, frames, offset, length = entry
        if kind not in (STARTUP, NORMAL):
            raise ValueError("unsupported track entry type")
        if not math.isfinite(duration) or duration < 0 or frames < 0:
            raise ValueError("invalid track duration or frame count")
        if offset < HEADER.size or length < COMMAND.size or offset + length > directory_offset:
            raise ValueError("track extent is outside demo frame data")
        entries.append(entry)
    extents = sorted((entry[6], entry[6] + entry[7]) for entry in entries)
    if any(previous[1] > current[0] for previous, current in zip(extents, extents[1:])):
        raise ValueError("overlapping track extents")
    samples = []
    network_frames = 0
    duration = 0.0
    network_duration = 0.0
    for entry in entries:
        track_samples, track_frames, track_network_duration = _read_track(data, entry)
        if entry[0] == NORMAL:
            samples.extend(track_samples)
            network_frames += track_frames
            duration += entry[4]
            network_duration += track_network_duration
    if len(samples) < min_network_frames:
        raise ValueError(f"insufficient nonempty network frames: {len(samples)}")
    if network_duration < min_duration:
        raise ValueError(f"insufficient network sample duration: {network_duration}")
    alive_samples = [sample for sample in samples if sample[0] > 0]
    span = [max(sample[1][axis] for sample in samples) - min(sample[1][axis] for sample in samples)
            for axis in range(3)]
    movement_samples = sum(any(value != 0.0 for value in sample[2]) for sample in alive_samples)
    alive_span = [max((sample[1][axis] for sample in alive_samples), default=0.0)
                  - min((sample[1][axis] for sample in alive_samples), default=0.0) for axis in range(3)]
    if require_alive and not alive_samples:
        raise ValueError("no recorded alive player samples")
    if require_movement and (not movement_samples or not any(value > 0.0 for value in alive_span)):
        raise ValueError("no recorded alive player movement command and simulated origin change")
    return {
        "map": _text(map_name),
        "game_directory": _text(game),
        "map_crc": map_crc,
        "sha256": hashlib.sha256(data).hexdigest(),
        "bytes": len(data),
        "tracks": entry_count,
        "network_frames": network_frames,
        "nonempty_network_frames": len(samples),
        "distinct_incoming_sequences": len({sample[3] for sample in samples}),
        "duration_seconds": duration,
        "network_sample_duration_seconds": network_duration,
        "alive_samples": len(alive_samples),
        "alive_movement_command_samples": movement_samples,
        "simulated_origin_span": span,
        "scope": "asset structure and recorded player state; engine playback and sign-on require runtime evidence",
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("demo", type=Path)
    parser.add_argument("--min-network-frames", type=int, default=1)
    parser.add_argument("--min-duration", type=float, default=0.0)
    parser.add_argument("--require-alive", action="store_true")
    parser.add_argument("--require-movement", action="store_true")
    args = parser.parse_args(argv)
    try:
        result = validate_demo(args.demo.read_bytes(), min_network_frames=args.min_network_frames,
                               min_duration=args.min_duration, require_alive=args.require_alive,
                               require_movement=args.require_movement)
    except (OSError, ValueError) as error:
        parser.exit(1, f"demo validation failed: {error}\n")
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
