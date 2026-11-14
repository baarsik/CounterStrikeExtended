import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('probe', Path(__file__).resolve().parents[3] / 'tools/qualification/extended_stats_probe.py')
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


def log():
    text = 'CSE_PROBE init health_id=100 armor_id=101 invalid_identity=0\n'
    for value in probe.VALUES:
        text += f'CSE_PROBE case subject=1 expected={value} health={value} armor={value} alive={int(value > 0)}\n'
        for name in ('HealthEx', 'BatteryEx'):
            text += f'CSE_PROBE wire name={name} dest=9 recipient=0 subject=1 amount={value} time=1.000\n'
    return text + 'CSE_PROBE complete subject=1\n'


class ProbeTests(unittest.TestCase):
    def test_complete_server_evidence(self):
        self.assertTrue(probe.analyze(log())['server_probe_pass'])

    def test_missing_wire_and_wrong_destination(self):
        self.assertFalse(probe.analyze(log().replace('amount=65536', 'amount=65535'))['server_probe_pass'])
        self.assertFalse(probe.analyze(log().replace('dest=9', 'dest=1'))['server_probe_pass'])

    def test_restart_cannot_reuse_old_success(self):
        text = log() + 'CSE_PROBE init health_id=100 armor_id=101 invalid_identity=0\n'
        self.assertFalse(probe.analyze(text)['server_probe_pass'])

    def test_failed_plugin_on_next_map_cannot_reuse_old_success(self):
        text = log() + '-------- Mapchange to de_dust2 --------\n[AMXX] Plugin failed to load\n'
        self.assertFalse(probe.analyze(text)['server_probe_pass'])

    def test_buffered_legacy_destination_and_order(self):
        evidence = log() + 'CSE_PROBE legacy dest=8 recipient=1 amount=100000\nCSE_PROBE legacy dest=8 recipient=1 amount=999\n'
        self.assertTrue(probe.analyze(evidence)['buffered_legacy_correction'])
        self.assertFalse(probe.analyze(evidence.replace('dest=8 recipient=1 amount=999', 'dest=1 recipient=1 amount=999'))['buffered_legacy_correction'])

    def test_no_downstream_acceptance_inference(self):
        result = probe.analyze(log())
        self.assertEqual(result['client_visual_acceptance'], 'unverified')
        self.assertEqual(result['hltv_late_join_and_demo_acceptance'], 'unverified')


if __name__ == '__main__':
    unittest.main()
