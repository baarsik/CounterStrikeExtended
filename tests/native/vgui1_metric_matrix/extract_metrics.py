"""Extract the unchanged candidate metric conversion for diagnostic reuse."""
import hashlib
import json
from pathlib import Path
import sys

source = Path(sys.argv[1])
output = Path(sys.argv[2])
output.mkdir(parents=True, exist_ok=True)
raw = source.read_bytes()
text = raw.decode('utf8')
constant = text[text.index('constexpr std::int64_t kFixedScale'):text.index('\n\nbool ToInt')]
conversion = text[text.index('bool ToInt('):text.index('bool RoundPositive(')]
start = text.index('std::int64_t Floor26(')
end = text.index('\n}\n}\n\nstruct HarmonyCandidate::Impl', start) + 2
conversion += text[start:end]
(output / 'actual_metric_conversion.inc').write_text(constant + '\n\n' + conversion + '\n')
(output / 'metric-source.json').write_text(json.dumps({'origin': str(source),
    'sha256': hashlib.sha256(raw).hexdigest(),
    'adaptation': 'Exact kFixedScale/ToInt/Floor26/Ceil26/GlyphMetrics bodies only'}, indent=2) + '\n')
