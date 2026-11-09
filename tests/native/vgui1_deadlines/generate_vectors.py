"""Independent arbitrary-precision signed32 oracle; no production code import."""
from pathlib import Path
import hashlib
import json
import random

rng = random.Random(0xC5EDED1)
modulus = 1 << 32
signed = lambda value: (value + (1 << 31)) % modulus - (1 << 31)
values = []
for clock in [0, 1, 0x7fffffff, 0x80000000, 0xffffffff]:
    for interval in [0, 1, 50, 400, -1, 0x7fffffff, -0x80000000]:
        values.append((clock, interval))
for _ in range(512):
    values.append((rng.getrandbits(32), signed(rng.getrandbits(32))))
text = '// Generated independently by Python arbitrary-precision arithmetic.\n'
text += 'struct TickVector {\n    std::uint32_t clock;\n    int interval;\n    std::int32_t expected;\n};\n'
text += 'constexpr std::int32_t MinimumTickValue = (std::numeric_limits<std::int32_t>::min)();\n'
text += 'constexpr TickVector TickVectors[] = {\n'
literal = lambda value: 'MinimumTickValue' if value == -(1 << 31) else str(value)
for clock, interval in values:
    text += f'    {{{clock}u, {literal(interval)}, {literal(signed(clock + interval))}}},\n'
text += '};\n'
Path(__file__).with_name('tick_vectors.h').write_text(text)
print(json.dumps({'vectors': len(values), 'seed': '0xC5EDED1', 'sha256': hashlib.sha256(text.encode()).hexdigest()}))
