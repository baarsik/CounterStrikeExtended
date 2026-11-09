"""Select the actual launcher source closure through the retained manifest evaluator."""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'filesystem'))
from manifest_sources import sources

root = Path(sys.argv[1]).resolve()
selected, _ = sources(root, sys.argv[2], root / 'GoldSrc-SDK/launcher/launcher.vcxproj')
Path(sys.argv[3]).write_text('set(CSE_LAUNCHER_SOURCES\n' + ''.join(
    f'  "{Path(source).as_posix()}"\n' for source in selected) + ')\n')
