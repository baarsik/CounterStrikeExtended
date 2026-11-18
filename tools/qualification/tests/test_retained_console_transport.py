"""Private GUI AllocConsole controls in Session0; never launches a game."""
import argparse
import json
from pathlib import Path
import sys
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / 'tests'))
from server_lifecycle_probe import ConsoleProcess, RetainedConsoleProcess, await_text
from test_console_transport import own_children

EVIDENCE = None
GUI = None
READER = None
AUDIT = []


class RetainedTests(unittest.TestCase):
    def setUp(self):
        if EVIDENCE is None:
            self.skipTest('Run this script with --evidence and both Windows console helpers')
        self.before = own_children()
        self.processes = []
        self.case = self.id().split('.')[-1]

    def tearDown(self):
        for process in reversed(self.processes):
            process.close()
        deadline = time.monotonic() + 3
        while own_children() != self.before and time.monotonic() < deadline:
            time.sleep(0.05)
        self.assertEqual(own_children(), self.before)

    def start(self, suffix=''):
        report = EVIDENCE / (self.case + suffix + '.json')
        process = RetainedConsoleProcess([GUI, report], EVIDENCE, EVIDENCE / (self.case + suffix + '.snapshot'))
        self.processes.append(process)
        self.assertEqual(process.session_id, 0)
        await_text(process, lambda text: 'CSE_GUI_CONSOLE_READY' in text, 'private GUI console readiness', timeout=5)
        return process, report

    def start_reader(self, gated=False):
        report = EVIDENCE / (self.case + '.lines')
        process = RetainedConsoleProcess([READER, report, 'gated' if gated else 'running'], EVIDENCE,
                                         EVIDENCE / (self.case + '.snapshot'))
        self.processes.append(process)
        await_text(process, lambda text: 'CSE_RETAINED_READER_READY' in text,
                   'actual retained reader readiness', timeout=5)
        return process, report

    def test_actual_reader_serialized_batch(self):
        process, report = self.start_reader()
        expected = ['first', 'second', 'third', 'quit']
        process.send(expected)
        self.assertEqual(process.wait(5), 0)
        self.assertEqual(report.read_text().splitlines(), expected)
        AUDIT.append({'case': self.case, 'actual_lines': expected, 'reader': 'exact production bodies'})

    def test_gui_allocates_separate_console_outside_conpty(self):
        report = EVIDENCE / 'conpty-gui.json'
        process = ConsoleProcess([GUI, report], EVIDENCE, EVIDENCE / 'conpty-gui.raw')
        self.processes.append(process)
        deadline = time.monotonic() + 5
        while not report.exists() and time.monotonic() < deadline:
            time.sleep(0.01)
        self.assertTrue(report.exists())
        result = json.loads(report.read_text())
        self.assertTrue(result['allocated_console'])
        self.assertTrue(result['input_console_mode'])
        self.assertTrue(result['output_console_mode'])
        with self.assertRaises(TimeoutError):
            process.wait(0.1)
        self.assertNotIn('CSE_GUI_CONSOLE_READY', process.text())
        process.close()
        self.assertTrue(process.forced, 'separate GUI console does not receive the ConPTY quit command')
        AUDIT.append({'case': self.case, 'observed': result, 'expected_unusable_conpty_channel': True})

    def test_gui_allocated_console_roundtrip(self):
        process, report = self.start()
        process.send(['CSE_GUI_INPUT'])
        self.assertEqual(process.wait(5), 0)
        process.close()
        result = json.loads(report.read_text())
        self.assertTrue(result['allocated_console'])
        self.assertTrue(result['input_console_mode'])
        self.assertTrue(result['output_console_mode'])
        self.assertEqual(result['received_line'], 'CSE_GUI_INPUT')
        AUDIT.append({'case': self.case, 'observed': result, 'console': process.console_observation})

    def test_gui_console_cooperative_close(self):
        process, report = self.start()
        process.close()
        self.assertFalse(process.forced)
        self.assertEqual(process.exit_code, 0)
        self.assertEqual(json.loads(report.read_text())['received_line'], 'quit')
        process.close()
        AUDIT.append({'case': self.case, 'console': process.console_observation})

    def test_two_owned_consoles_remain_independent(self):
        first, first_report = self.start('-first')
        second, second_report = self.start('-second')
        self.assertNotEqual(first.console_observation['window'], second.console_observation['window'])
        first.send(['CSE_GUI_INPUT'])
        second.send(['quit'])
        self.assertEqual(first.wait(5), 0)
        self.assertEqual(second.wait(5), 0)
        self.assertEqual(json.loads(first_report.read_text())['received_line'], 'CSE_GUI_INPUT')
        self.assertEqual(json.loads(second_report.read_text())['received_line'], 'quit')
        AUDIT.append({'case': self.case, 'consoles': [first.console_observation, second.console_observation]})


def main():
    global EVIDENCE, GUI, READER
    parser = argparse.ArgumentParser()
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--gui-helper', type=Path, required=True)
    parser.add_argument('--reader-helper', type=Path, required=True)
    args = parser.parse_args()
    EVIDENCE = args.evidence
    GUI = args.gui_helper
    READER = args.reader_helper
    EVIDENCE.mkdir(parents=True, exist_ok=False)
    result = unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(
        unittest.defaultTestLoader.loadTestsFromTestCase(RetainedTests))
    (EVIDENCE / 'result.json').write_text(json.dumps({'success': result.wasSuccessful(),
                                                   'tests': result.testsRun, 'audit': AUDIT}, indent=2) + '\n')
    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    raise SystemExit(main())
