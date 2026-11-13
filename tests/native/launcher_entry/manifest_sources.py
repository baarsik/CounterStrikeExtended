"""Select the actual launcher source closure through its owning CMake target."""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'filesystem'))
from manifest_sources import sources

root = Path(sys.argv[1]).resolve()
architecture = sys.argv[4] if len(sys.argv) > 4 else 'x86'
selected, evaluated = sources(root, sys.argv[2], 'launcher', architecture)
standards = {value['properties']['CXX_STANDARD'] for value in evaluated.values()}
if len(standards) != 1:
    raise ValueError('Launcher C++ standard differs between configurations')
standard = standards.pop()
Path(sys.argv[3]).write_text('set(CSE_LAUNCHER_SOURCES\n' + ''.join(
    f'  "{Path(source).as_posix()}"\n' for source in selected) + ')\n'
    + f'set(CSE_LAUNCHER_CXX_STANDARD {standard})\n')
