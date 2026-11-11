"""Bounded malformed-trie controls for the source-owned static reader."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / 'tools'))
from main_image_publication import decode_exports


class TrieTests(unittest.TestCase):
    def test_regular_and_foreign_reexport_records_remain_distinct(self):
        edge = b'_symbol\0'
        prefix = b'\0\1' + edge + bytes([len(edge) + 3])
        self.assertEqual(decode_exports(prefix + b'\2\0\x42\0'),
                         {'_symbol': {'flags': 0, 'address': 66}})
        self.assertEqual(decode_exports(prefix + b'\3\x08\1\0\0'),
                         {'_symbol': {'flags': 8, 'ordinal': 1, 'import': ''}})

    def test_truncated_overflowing_and_cyclic_tries_fail_closed(self):
        for trie in (b'\x80', b'\x80' * 10 + b'\2', b'\0\1x\0\0', b'\0\1x\0\xff\1',
                     b'\0\1unterminated', b'\2\0'):
            with self.subTest(trie=trie), self.assertRaises(ValueError):
                decode_exports(trie)


if __name__ == '__main__':
    unittest.main()
