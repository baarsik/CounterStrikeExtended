"""Checks for the fixed-demo macOS Metal benchmark analyzer."""

import importlib.util
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "analyze_metal_benchmark.py"
SPEC = importlib.util.spec_from_file_location("analyze_metal_benchmark", SCRIPT)
analyzer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(analyzer)


class MetalBenchmarkAnalysisTests(unittest.TestCase):
    def make_run(self, root, name, intervals, *, unfocused=-1, complete=True):
        run = root / name
        run.mkdir()
        lines = ["# pid=123", "interval_ms,connected,menu,focused,completed"]
        for index, interval in enumerate(intervals):
            lines.append(f"{interval},1,0,{int(index != unfocused)},1")
        (run / "frames.csv").write_text("\n".join(lines) + "\n")
        count = "6" if complete else "5"
        (run / "game.log").write_text(
            "Playing demo from fixed.dem.\n" + f"{count} frames 0.02 seconds 300.0 fps\n"
        )
        return run

    def test_fixed_demo_metrics_and_paired_change(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            baseline = self.make_run(root, "baseline", [1, 4, 4, 4, 4, 4])
            candidate = self.make_run(root, "candidate", [1, 2, 2, 2, 2, 2])
            before = analyzer.analyze_run(baseline, "fixed.dem", 6, warmup=1)
            after = analyzer.analyze_run(candidate, "fixed.dem", 6, warmup=1)
            comparison = analyzer.compare(before, after)
            self.assertEqual(before["analyzed_frames"], 5)
            self.assertEqual(before["fps"], 250.0)
            self.assertEqual(before["p95_ms"], 4.0)
            self.assertEqual(before["one_percent_low_fps"], 250.0)
            self.assertEqual(comparison["fps_change_percent"], 100.0)
            self.assertEqual(comparison["p95_ms_change_percent"], -50.0)

    def test_focus_loss_invalidates_run(self):
        with tempfile.TemporaryDirectory() as directory:
            run = self.make_run(Path(directory), "unfocused", [1, 2, 2, 2, 2, 2], unfocused=2)
            with self.assertRaisesRegex(ValueError, "focus"):
                analyzer.analyze_run(run, "fixed.dem", 6, warmup=1)

    def test_truncated_demo_invalidates_run(self):
        with tempfile.TemporaryDirectory() as directory:
            run = self.make_run(Path(directory), "short", [1, 2, 2, 2, 2, 2], complete=False)
            with self.assertRaisesRegex(ValueError, "frame count"):
                analyzer.analyze_run(run, "fixed.dem", 6, warmup=1)


if __name__ == "__main__":
    unittest.main()
