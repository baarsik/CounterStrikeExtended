"""Boundary and evidence-rejection tests for integer-FPS paired replay analysis."""
import math
import hashlib
import json
import tempfile
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from analyze_benchmark import DEMOS, analyze_series, frame_bounds_ms, summarize_profiles, parse_replay, paired_summary


def replay(fps=(100, 0, 999), frames=650):
    profiles = ''.join(f'noise{f} fps -- host(  1) sv(  0) cl(  1) gfx(  2) snd(  0) ents(0)\n' for f in fps)
    return ('CSE_BENCH_START\n' + profiles + 'Host_EndGame: End of demo.\n'
            f'{frames} frames 3.250 seconds 200.000 fps\nCSE_BENCH_DONE\n')


class BenchmarkTests(unittest.TestCase):
    def test_fps_quantization_and_fast_sentinel(self):
        self.assertEqual(frame_bounds_ms(100), (1000 / 101, 10.0))
        self.assertEqual(frame_bounds_ms(999), (0.0, 1000 / 999))
        self.assertEqual(frame_bounds_ms(0), (1000.0, math.inf))
        with self.assertRaises(ValueError):
            frame_bounds_ms(-1)

    def test_zero_frames_not_discarded_and_unbounded_json_value(self):
        result = summarize_profiles([100] * 98 + [999, 0])
        self.assertEqual(result['count'], 100)
        self.assertEqual(result['zero_fps_count'], 1)
        self.assertEqual(result['fps_999_count'], 1)
        self.assertEqual(result['quantiles_ms']['p99']['upper'], 10.0)
        self.assertIsNone(result['maximum_ms']['upper'])
        self.assertEqual(result['maximum_ms']['lower'], 1000.0)
        slow = summarize_profiles([0] * 100)
        self.assertIsNone(slow['quantiles_ms']['p95']['upper'])
        self.assertIsNone(slow['quantiles_ms']['p95']['midpoint_estimate'])

    def test_nonnewline_profile_prefix_and_one_extra_host_frame(self):
        result = parse_replay(replay([100] * 651))
        self.assertEqual(result['profiles']['count'], 651)
        self.assertEqual(result['cli']['frames'], 650)

    def test_rejects_missing_eof_even_with_exit_and_final_marker(self):
        with self.assertRaisesRegex(ValueError, 'EOF'):
            parse_replay(replay([100] * 650).replace('Host_EndGame: End of demo.', ''))

    def test_rejects_partial_or_bad_profile_coverage(self):
        for content in (replay([100] * 499, 499), replay([100] * 10), replay([100] * 655)):
            with self.subTest(content=content[:30]), self.assertRaises(ValueError):
                parse_replay(content)

    def test_rejects_duplicate_markers_or_reports(self):
        content = replay([100] * 650)
        for suffix in ('CSE_BENCH_DONE', '650 frames 3.250 seconds 200.000 fps'):
            with self.assertRaises(ValueError):
                parse_replay(content + suffix)

    def test_paired_changes_use_ratios_per_pair(self):
        pairs = [(100.0, 110.0), (200.0, 200.0), (50.0, 60.0)]
        result = paired_summary(pairs)
        self.assertAlmostEqual(result['median_paired_fps_change_percent'], 10.0)
        self.assertEqual(result['baseline_median_cli_fps'], 100.0)
        self.assertEqual(result['candidate_median_cli_fps'], 110.0)
        self.assertEqual(result['paired_fps_change_percent'], [10.000000000000009, 0.0, 19.999999999999996])


class SeriesTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.path = self.root / 'cstrike-test-series.json'
        self.series = {'success': True, 'game': 'cstrike', 'demo_sha256': DEMOS['cstrike'],
                       'pairs': 1, 'runs': [], 'instrumentation': 'test fixture',
                       'settings_sha256': hashlib.sha256(b'fixed settings').hexdigest()}
        (self.root / 'cstrike-test-settings.cfg').write_bytes(b'fixed settings')
        for pair, versions in ((0, ('baseline', 'candidate')), (1, ('candidate', 'baseline'))):
            for version in versions:
                prefix = f'cstrike-test-p{pair}-{version}'
                name = prefix + '-result.json'
                result = {'success': True, 'exit_code': 0, 'game': 'cstrike',
                          'demo_sha256': DEMOS['cstrike'], 'pair': pair, 'version': version,
                          'warmup': pair == 0, 'series': self.path.name, 'prefix': prefix,
                          'pid': 123, 'frames': 650, 'seconds': 3.25, 'fps': 200.0,
                          'profile_count': 650, 'wall_seconds': 9.0,
                          'window_samples': [{'owned_foreground': True, 'visible': True,
                                              'minimized': False, 'foreground_pid': 123, 'elapsed_ms': 1000}]}
                (self.root / name).write_text(json.dumps(result))
                (self.root / (prefix + '-qconsole-0.log')).write_text(replay([100] * 650))
                self.series['runs'].append(name)
        self.save()

    def save(self):
        self.path.write_text(json.dumps(self.series))

    def mutate_run(self, **values):
        path = self.root / self.series['runs'][0]
        result = json.loads(path.read_text())
        result.update(values)
        path.write_text(json.dumps(result))

    def test_valid_smoke_preserves_warmups_but_excludes_from_summary(self):
        result = analyze_series(self.path)
        self.assertEqual(len(result['runs']), 4)
        self.assertEqual(result['summary']['pair_count'], 1)
        self.assertEqual(result['qualification'], 'smoke only')

    def test_rejects_missing_reordered_or_duplicate_run(self):
        for runs in (self.series['runs'][:-1], self.series['runs'][::-1], self.series['runs'] * 2):
            with self.subTest(runs=runs):
                self.series['runs'] = runs
                self.save()
                with self.assertRaises(ValueError):
                    analyze_series(self.path)

    def test_rejects_forged_summary_or_wrong_demo_or_forced_cleanup(self):
        for field in ({'fps': 250.0}, {'demo_sha256': '0' * 64}, {'forced_cleanup': True}):
            with self.subTest(field=field):
                self.mutate_run(fps=200.0, demo_sha256=DEMOS['cstrike'], forced_cleanup=False)
                self.mutate_run(**field)
                with self.assertRaises(ValueError):
                    analyze_series(self.path)

    def test_rejects_shared_settings_mutation_and_hidden_replay(self):
        settings = self.root / 'cstrike-test-settings.cfg'
        settings.write_bytes(b'changed settings')
        with self.assertRaisesRegex(ValueError, 'settings hash'):
            analyze_series(self.path)
        settings.write_bytes(b'fixed settings')
        self.mutate_run(window_samples=[
            {'owned_foreground': True, 'visible': True, 'minimized': False,
             'foreground_pid': 123, 'elapsed_ms': 1000},
            {'owned_foreground': False, 'visible': False, 'minimized': False,
             'foreground_pid': 124, 'elapsed_ms': 2000}])
        with self.assertRaisesRegex(ValueError, 'hidden/background'):
            analyze_series(self.path)

    def test_rejects_background_or_foreground_pid_mismatch(self):
        self.mutate_run(window_samples=[{'owned_foreground': True, 'visible': True,
                                         'minimized': False, 'foreground_pid': 124, 'elapsed_ms': 1000}])
        with self.assertRaisesRegex(ValueError, 'PID'):
            analyze_series(self.path)
        self.mutate_run(window_samples=[{'owned_foreground': False, 'visible': True,
                                         'minimized': False, 'foreground_pid': 124, 'elapsed_ms': 1000}])
        with self.assertRaisesRegex(ValueError, 'foreground'):
            analyze_series(self.path)


if __name__ == '__main__':
    unittest.main()
