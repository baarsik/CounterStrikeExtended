"""Compare fixed-demo macOS frame traces from CSE_FRAME_TIME_LOG.

Each run directory must contain frames.csv and game.log from a separate, complete
`-nosound` replay. This reports wall-clock frame intervals, not GPU execution time.
"""

import argparse
import csv
import json
import math
import re
import statistics
from pathlib import Path


def percentile(values, proportion):
    ordered = sorted(values)
    return ordered[math.ceil(len(ordered) * proportion) - 1]


def analyze_run(directory, expected_demo, expected_frames, warmup=500):
    directory = Path(directory)
    log = (directory / "game.log").read_text(errors="replace")
    if f"Playing demo from {expected_demo}" not in log:
        raise ValueError(f"{directory}: expected demo was not played")
    reports = re.findall(r"(\d+) frames [0-9.]+ seconds [0-9.]+ fps", log)
    if not reports or int(reports[-1]) != expected_frames:
        raise ValueError(f"{directory}: incomplete or unexpected demo frame count")

    with (directory / "frames.csv").open(newline="") as stream:
        rows = list(csv.DictReader(line for line in stream if not line.startswith("#")))
    active = [row for row in rows if row["connected"] == "1" and row["completed"] == "1"]
    if len(active) < math.ceil(expected_frames * 0.99) or len(active) <= warmup:
        raise ValueError(f"{directory}: insufficient connected frame samples")
    measured = active[warmup:]
    if any(row["focused"] != "1" or row["menu"] != "0" for row in measured):
        raise ValueError(f"{directory}: focus or menu state changed during replay")
    intervals = [float(row["interval_ms"]) for row in measured]
    if any(not math.isfinite(value) or value <= 0 for value in intervals):
        raise ValueError(f"{directory}: invalid frame interval")

    slowest_count = math.ceil(len(intervals) * 0.01)
    slowest = sorted(intervals)[-slowest_count:]
    return {
        "directory": str(directory.resolve()),
        "demo": expected_demo,
        "reported_frames": expected_frames,
        "active_frames": len(active),
        "analyzed_frames": len(intervals),
        "fps": 1000.0 / statistics.mean(intervals),
        "one_percent_low_fps": 1000.0 / statistics.mean(slowest),
        "p50_ms": statistics.median(intervals),
        "p95_ms": percentile(intervals, 0.95),
        "p99_ms": percentile(intervals, 0.99),
    }


def compare(baseline, candidate):
    if baseline["demo"] != candidate["demo"] or baseline["reported_frames"] != candidate["reported_frames"]:
        raise ValueError("benchmark runs used different demos")
    return {
        "fps_change_percent": (candidate["fps"] / baseline["fps"] - 1.0) * 100.0,
        "one_percent_low_fps_change_percent":
            (candidate["one_percent_low_fps"] / baseline["one_percent_low_fps"] - 1.0) * 100.0,
        "p95_ms_change_percent": (candidate["p95_ms"] / baseline["p95_ms"] - 1.0) * 100.0,
        "p99_ms_change_percent": (candidate["p99_ms"] / baseline["p99_ms"] - 1.0) * 100.0,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--demo", required=True, help="Demo filename shown in the game log")
    parser.add_argument("--frames", required=True, type=int, help="Expected completed demo frame count")
    parser.add_argument("--warmup", type=int, default=500, help="Connected frames excluded at the start")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.frames <= 0 or args.warmup < 0:
        parser.error("--frames must be positive and --warmup must be nonnegative")
    baseline = analyze_run(args.baseline, args.demo, args.frames, args.warmup)
    candidate = analyze_run(args.candidate, args.demo, args.frames, args.warmup)
    result = {"baseline": baseline, "candidate": candidate, "comparison": compare(baseline, candidate)}
    rendered = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.write_text(rendered)
    print(rendered, end="")


if __name__ == "__main__":
    main()
