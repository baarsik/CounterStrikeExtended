import contextlib
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('image_to_background', ROOT / 'src/devtools/image_to_background.py')
background = importlib.util.module_from_spec(spec)
spec.loader.exec_module(background)


class BackgroundWebPTests(unittest.TestCase):
    def test_tiles_are_lossless_webp_and_layout_names_match(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            image = Image.new('RGBA', (257, 2), (12, 34, 56, 128))
            image.putpixel((256, 1), (255, 1, 2, 255))
            source = root / 'source.png'
            image.save(source)
            with contextlib.redirect_stdout(io.StringIO()):
                background.convert_to_tiles(str(source), str(root / 'output'))
            tiles = sorted((root / 'output/background').iterdir())
            self.assertEqual([p.suffix for p in tiles], ['.webp', '.webp'])
            layout = (root / 'output/BackgroundLayout.txt').read_text()
            for tile in tiles:
                self.assertIn(tile.name, layout)
                with Image.open(tile) as actual:
                    self.assertEqual(actual.format, 'WEBP')
                    expected = image.crop((0, 0, 256, 2) if actual.width == 256 else (256, 0, 257, 2))
                    self.assertEqual(actual.convert('RGBA').tobytes(), expected.tobytes())
            self.assertEqual(layout, (root / 'output/BackgroundLoadingLayout.txt').read_text())


if __name__ == '__main__':
    unittest.main()
