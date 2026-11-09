#!/usr/bin/env python3
"""Fail-closed parser controls against a captured real compiler report."""
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
import vgui1_app_cold_evidence as evidence


class ReportTests(unittest.TestCase):
    def setUp(self):
        folder = Path(__file__).with_name('reports')
        self.report = (folder / 'msvc-x86-app-include-interleave.txt').read_text()
        self.symbols = (folder / 'msvc-x86-app-symbols.txt').read_text()
        self.schema = json.loads((ROOT / 'tools/vgui1_app_cold_evidence_schema.json').read_text())

    def test_real_msvc_midline_include_diagnostic_preserves_all_adjustors(self):
        slots = evidence.physical_slots(self.report, self.symbols, True, self.schema)
        self.assertEqual(len(slots), 55)
        self.assertEqual(slots[15]['name'], 'setMouseArena')
        self.assertEqual(slots[15]['id'], 17)
        self.assertEqual(slots[16]['id'], 16)

    def test_nonzero_adjustor_is_never_normalized(self):
        report = self.report.replace('vgui::App::init this adjustor: 0', 'vgui::App::init this adjustor: 4')
        with self.assertRaises(evidence.common.EvidenceError):
            evidence.physical_slots(report, self.symbols, True, self.schema)

    def test_unknown_or_truncated_reports_are_not_accepted(self):
        for text, symbols in [('unknown compiler format', self.symbols), (self.report, 'unknown symbols'),
                              (self.report.replace('48\t| &vgui::App::init', '48\t| &vgui::App::newUnknownVirtual'), self.symbols)]:
            with self.assertRaises(evidence.common.EvidenceError):
                evidence.physical_slots(text, symbols, True, self.schema)


if __name__ == '__main__':
    unittest.main()
