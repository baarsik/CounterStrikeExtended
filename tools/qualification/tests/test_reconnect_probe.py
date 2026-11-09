"""Real-file and callback-correlation checks for the reconnect harness."""
import importlib.util
from pathlib import Path
import tempfile
import threading
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('reconnect_probe', ROOT / 'reconnect_probe.py')
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


class ReconnectHarnessTests(unittest.TestCase):
    def test_cfg_create_and_replacement_are_exact(self):
        with tempfile.TemporaryDirectory() as folder:
            game = Path(folder)
            probe.write_cfg(game, 'tick.cfg', ['echo FIRST'])
            self.assertTrue((game / 'tick.cfg').is_file())
            for number in range(100):
                commands = [f'echo NEXT_{number}', 'wait']
                probe.write_cfg(game, 'tick.cfg', commands)
                self.assertEqual((game / 'tick.cfg').read_bytes(), ('\n'.join(commands) + '\n').encode('ascii'))
            self.assertEqual(sorted(path.name for path in game.iterdir()), ['tick.cfg'])

    def test_cfg_rejects_limit_and_preserves_destination(self):
        with tempfile.TemporaryDirectory() as folder:
            game = Path(folder)
            (game / 'tick.cfg').write_bytes(b'previous\n')
            with self.assertRaises(ValueError):
                probe.write_cfg(game, 'tick.cfg', ['x' * 16384])
            self.assertEqual((game / 'tick.cfg').read_bytes(), b'previous\n')

    def test_cfg_rejects_escape_and_line_injection(self):
        with tempfile.TemporaryDirectory() as folder:
            for name, commands in [('../outside.cfg', ['echo X']), ('tick.cfg', ['echo X\nquit']),
                                    ('tick.cfg', ['echo \u0410'])]:
                with self.subTest(name=name, commands=commands):
                    with self.assertRaises(ValueError):
                        probe.write_cfg(Path(folder), name, commands)

    def test_stage_waits_for_its_own_ack_before_returning(self):
        with tempfile.TemporaryDirectory() as folder:
            console = Path(folder) / 'console.log'
            console.write_text('CSE_RECONNECT_STAGE_1_join\n')
            done = threading.Event()
            def consume():
                probe.wait_stage_marker(console.read_text, 'CSE_RECONNECT_STAGE_2_join', lambda: None, timeout=1)
                done.set()
            worker = threading.Thread(target=consume)
            worker.start()
            try:
                self.assertFalse(done.wait(0.05), 'old phase data must not acknowledge a new stage')
                console.write_text('CSE_RECONNECT_STAGE_1_join\nCSE_RECONNECT_STAGE_2_join\n')
                self.assertTrue(done.wait(1))
            finally:
                console.write_text('CSE_RECONNECT_STAGE_2_join\n')
                worker.join(2)

    def test_missing_stage_ack_times_out(self):
        with self.assertRaises(TimeoutError):
            probe.wait_stage_marker(lambda: 'old data', 'CSE_RECONNECT_STAGE_2_join', lambda: None, timeout=0.01)

    def test_exact_correlated_callbacks(self):
        lines = ['CSE_FIXTURE_AUTH id=2 steam_authorized=1', 'CSE_FIXTURE_STARTED id=2',
                 'CSE_FIXTURE_MENU_REPLY id=2 item=0',
                 'CSE_FIXTURE_CVAR_REPLY id=2 name=cl_updaterate value=37', 'CSE_FIXTURE_CLIENT_ACK id=2']
        self.assertEqual(probe.correlate_callbacks(lines), 2)

    def test_missing_fresh_auth_rejected(self):
        with self.assertRaises(ValueError):
            probe.correlate_callbacks(['CSE_FIXTURE_STARTED id=1', 'CSE_FIXTURE_CLIENT_ACK id=1'])

    def test_other_client_callbacks_do_not_satisfy_phase(self):
        with self.assertRaises(ValueError):
            probe.correlate_callbacks(['CSE_FIXTURE_AUTH id=1 steam_authorized=1',
                                       'CSE_FIXTURE_STARTED id=1', 'CSE_FIXTURE_MENU_REPLY id=2 item=0',
                                       'CSE_FIXTURE_CVAR_REPLY id=1 name=cl_updaterate value=37',
                                       'CSE_FIXTURE_CLIENT_ACK id=1'])

    def test_duplicate_auth_and_nonaccepted_menu_rejected(self):
        base = ['CSE_FIXTURE_AUTH id=1 steam_authorized=1', 'CSE_FIXTURE_STARTED id=1',
                'CSE_FIXTURE_MENU_REPLY id=1 item=-1',
                'CSE_FIXTURE_CVAR_REPLY id=1 name=cl_updaterate value=37', 'CSE_FIXTURE_CLIENT_ACK id=1']
        for lines in [base, base + [base[0]]]:
            with self.subTest(lines=lines):
                with self.assertRaises(ValueError):
                    probe.correlate_callbacks(lines)


if __name__ == '__main__':
    unittest.main()
