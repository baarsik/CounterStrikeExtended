import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('analyze_music_probe', Path(__file__).parents[1] / 'analyze_music_probe.py')
audit = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(audit)


class MusicCaptureTests(unittest.TestCase):
    def fixture(self, root):
        prefix = root / 'music'
        phases = dict(zip(['FULL','PAUSE','RESUME','LOW','FADE','FADED','STOCK','REPLACED','STOP','DONE'],
                          [5,8,10,12,14,15,17,19,21,23]))
        levels = {'FULL':0.4, 'PAUSE':0, 'RESUME':0.4, 'LOW':0.1, 'FADE':0.05,
                  'FADED':0, 'STOCK':0.2, 'REPLACED':0.24, 'STOP':0}
        pcm = bytearray()
        spans = []
        for (phase, start), (_, end) in zip(phases.items(), list(phases.items())[1:]):
            chunk = struct.pack('<f', levels[phase]) * ((end-start) * 1000)
            spans.append(f'{len(pcm)},{start},{len(chunk)}')
            pcm.extend(chunk)
        fmt = struct.pack('<HHIIHH', 3, 1, 1000, 4000, 4, 32)
        chunks = b'fmt ' + struct.pack('<I',len(fmt)) + fmt + b'data' + struct.pack('<I',len(pcm)) + pcm
        Path(str(prefix)+'.wav').write_bytes(b'RIFF'+struct.pack('<I',len(chunks)+4)+b'WAVE'+chunks)
        Path(str(prefix)+'-audio-spans.csv').write_text('\n'.join(spans)+'\n')
        Path(str(prefix)+'-result.json').write_text(json.dumps({'success':True,'phases':phases,'decoder_sha256':'fixture'}))
        Path(str(prefix)+'-console.log').write_text('Current MP3 Volume: 0.250000\nCSE_MUSIC_REPLACED\nCurrent MP3 Title: media/cse-music-test.mp3\nCSE_MUSIC_STOP\n')
        return prefix

    def test_capture_timestamps_preserve_initial_silence_offset(self):
        with tempfile.TemporaryDirectory() as directory:
            report = audit.analyze(self.fixture(Path(directory)))
            self.assertAlmostEqual(report['quarter_volume_ratio'],0.25,places=5)
            self.assertEqual(report['rms']['PAUSE'],0)
            self.assertEqual(report['rms']['FADED'],0)
            self.assertEqual(report['rms']['STOP'],0)

    def test_stock_music_cannot_satisfy_replacement_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = self.fixture(Path(directory))
            path = Path(str(prefix)+'-console.log')
            path.write_text(path.read_text().replace('media/cse-music-test.mp3','media/Half-Life01.mp3'))
            with self.assertRaisesRegex(ValueError,'replacement track'):
                audit.analyze(prefix)

    def test_wrong_volume_report_cannot_pass_audio_only_checks(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = self.fixture(Path(directory))
            Path(str(prefix)+'-console.log').write_text('Current MP3 Volume: 0\n')
            with self.assertRaisesRegex(ValueError,'quarter volume'):
                audit.analyze(prefix)


if __name__ == '__main__':
    unittest.main()
