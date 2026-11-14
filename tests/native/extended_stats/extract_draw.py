from pathlib import Path
import sys

root = Path(sys.argv[1])
output = Path(sys.argv[2])
source = root / 'src/cstrike/cl_dll/hud_stat_numbers.cpp'
if not source.exists():
    output.write_text('#define CSE_STAT_DRAW_MISSING\n')
else:
    text = source.read_text()
    bodies = []
    for name in ('int CHud::GetHudStatNumberWidth(', 'int CHud::DrawHudStatNumber('):
        begin = text.index(name)
        end = text.index('{', begin) + 1
        depth = 1
        while depth:
            depth += (text[end] == '{') - (text[end] == '}')
            end += 1
        bodies.append(text[begin:end])
    output.write_text('\n\n'.join(bodies))
spectator = (root / 'src/cstrike/cl_dll/VGUI/cstrikespectatorgui.cpp').read_text(encoding='latin1')
target = output.with_name('spectator_bodies.inc')
bodies = []
for signature in ('void CCSSpectatorGUI::GetPlayerLabelText(', 'void CCSSpectatorGUI::FormatStatsLabel(',
                  'void CCSSpectatorGUI::RefreshLanguage('):
    begin = spectator.index(signature)
    end = spectator.index('{', begin) + 1
    depth = 1
    while depth:
        depth += (spectator[end] == '{') - (spectator[end] == '}')
        end += 1
    bodies.append(spectator[begin:end])
target.write_text('\n\n'.join(bodies))
