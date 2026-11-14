from pathlib import Path
import sys
root = Path(sys.argv[1])
text = (root / 'src/cstrike/cl_dll/extended_stats.cpp').read_text()
def function(signature):
    begin = text.index(signature)
    end = text.index('{', begin) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[begin:end]
bodies = ['namespace cse::extstats {\n' + function('ClientState& ClientStats()') + '\n}']
for signature in ('int CSE_LocalPlayerIndex()', 'static bool ValidPlayer(', 'static void ApplyHealth(',
                  'bool CSE_HandleExtendedStats(', 'void CSE_UpdateLegacyHealth(',
                  'void CSE_UpdateLegacyArmor(', 'void CSE_ResetExtendedStats(', 'void CSE_ThinkExtendedStats('):
    bodies.append(function(signature))
Path(sys.argv[2]).write_text('\n\n'.join(bodies))

for filename, signature in (('health.cpp', 'int CHudHealth::MsgFunc_Health('),
                            ('battery.cpp', 'int CHudBattery::MsgFunc_Battery(')):
    text = (root / 'src/cstrike/cl_dll' / filename).read_text(encoding='latin1')
    bodies.append(function(signature))
Path(sys.argv[2]).write_text('\n\n'.join(bodies))
