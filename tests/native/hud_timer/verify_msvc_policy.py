"""Compile the emitted legacy body and prove its owning warning state cannot leak."""
import json
import os
from pathlib import Path
import subprocess
import sys


compiler = sys.argv[1]
build = Path(sys.argv[2]).resolve()
output = build / 'msvc-diagnostic-boundary'
output.mkdir(exist_ok=True)
# Keep this compiler control independent of optional command-line environment flags.
environment = {key: value for key, value in os.environ.items() if key.upper() not in ('CL', '_CL_')}
records = []
for scenario, suffix in (
    ('legacy', ''),
    ('outside', '\nfloat OutsideNarrowing(double value) { return value; }\n'),
):
    source = output / f'{scenario}.cpp'
    source.write_text('#include "scale_colors.inc"\n' + suffix)
    command = [compiler, '/nologo', '/c', '/W4', '/WX', '/I' + str(build),
               '/Fo' + str(output / f'{scenario}.obj'), str(source)]
    result = subprocess.run(command, env=environment, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True)
    log = output / f'{scenario}.log'
    log.write_text(result.stdout)
    records.append({'scenario': scenario, 'argv': command, 'exit': result.returncode, 'log': str(log)})
    print(scenario, json.dumps(command), 'exit', result.returncode)
    print(result.stdout)
    if scenario == 'legacy':
        passed = result.returncode == 0
    else:
        passed = result.returncode != 0 and 'C4244' in result.stdout
    records[-1]['expectedResultObserved'] = passed
(output / 'receipt.json').write_text(json.dumps(records, indent=2) + '\n')
if not all(record['expectedResultObserved'] for record in records):
    sys.exit('FAIL: legacy body must compile while outside narrowing must fail with C4244')
print('PASS MSVC legacy diagnostic context is scoped to ScaleColors')
