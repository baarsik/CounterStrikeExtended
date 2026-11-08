"""Audio qualification compares exact canonical bytes, without numeric tolerance."""
import hashlib
import importlib.util
from pathlib import Path
import unittest

MODULE = Path(__file__).parents[1] / 'verify_pcm.py'
if MODULE.exists():
    spec = importlib.util.spec_from_file_location('verify_pcm', MODULE)
    verifier = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(verifier)
else:
    verifier = None


class PCMReferenceTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(verifier, 'exact PCM verifier is required')
        self.encoded = b'encoded canonical input'
        self.pcm = b'\0\0\1\0\xff\x7f'
        self.reference = {'schema': 1, 'encoded_sha256': hashlib.sha256(self.encoded).hexdigest(),
                          'pcm_sha256': hashlib.sha256(self.pcm).hexdigest(), 'pcm_bytes': len(self.pcm)}

    def test_exact_input_and_pcm_pass(self):
        verifier.verify(self.encoded, self.pcm, self.reference)

    def test_one_sample_bit_or_input_change_cannot_pass(self):
        for encoded, pcm in [(self.encoded + b'x', self.pcm),
                             (self.encoded, self.pcm[:-2] + b'\xfe\x7f'), (self.encoded, self.pcm[:-2])]:
            with self.subTest(pcm=pcm), self.assertRaises(ValueError):
                verifier.verify(encoded, pcm, self.reference)


if __name__ == '__main__':
    unittest.main()
