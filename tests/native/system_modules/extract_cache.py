from pathlib import Path
import sys, hashlib
source = Path(sys.argv[1]).read_text()
out = '#include "fixture.hpp"\nstatic IDemoPlayer* s_DemoPlayer;\nstatic int s_DemoPlayerMode;\n'
for signature in ['void DemoPlayer_Reset(', 'void DemoPlayer_SetActive(', 'int DemoPlayer_IsActive(', 'int DemoPlayer_Init(']:
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    body = source[start:end]
    out += '// SHA256 source region: ' + hashlib.sha256(body.encode()).hexdigest() + '\n' + body + '\n'
Path(sys.argv[2]).write_text(out)
