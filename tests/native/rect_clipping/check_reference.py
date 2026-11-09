#!/usr/bin/env python3
"""Independent unbounded-integer intersection oracle; OS float results remain the golden."""
import argparse
import collections
import json
from pathlib import Path

parser=argparse.ArgumentParser()
parser.add_argument('--art',type=Path,required=True)
args=parser.parse_args();art=args.art
paths=sorted((art/'windows').glob('*-windows-reference.jsonl'))
assert len(paths)==6,len(paths)
golden=[json.loads(line) for line in paths[0].read_text().splitlines()]
for path in paths[1:]:
    rows=[json.loads(line) for line in path.read_text().splitlines()]
    assert rows==golden, f'Windows cross-profile disagreement: {path}'
for row in golden:
    if row['kind']!='intersection':continue
    a=row['a'];b=row['b']
    out=[max(a[0],b[0]),max(a[1],b[1]),min(a[2],b[2]),min(a[3],b[3])]
    ready=out[0]<out[2] and out[1]<out[3]
    assert row['result']==int(ready) and row['out']==(out if ready else [0,0,0,0]),row
report=[]
for path in sorted((art/'windows').glob('*-portable-baseline.jsonl')):
    rows=[json.loads(line) for line in path.read_text().splitlines()]
    mismatches=[{'row':index,'expected':expected,'actual':actual}
        for index,(expected,actual) in enumerate(zip(golden,rows)) if expected!=actual]
    assert mismatches, f'expected native empty-rectangle RED missing: {path}'
    counts=collections.Counter(row['expected']['kind'] for row in mismatches)
    report.append({'context':path.name,'row_count':len(rows),'mismatches':len(mismatches),'kinds':dict(counts)})
    (art/(path.stem+'-mismatches.json')).write_text(json.dumps(mismatches,indent=2)+'\n')
(art/'reference-summary.json').write_text(json.dumps({'windows_profiles_equal':6,'rows_per_profile':len(golden),
    'oracle_intersection_rows':sum(row['kind']=='intersection' for row in golden),'baseline_red':report},indent=2)+'\n')
(art/'golden.jsonl').write_text(''.join(json.dumps(row,separators=(',',':'))+'\n' for row in golden))
print(json.dumps(report,indent=2))
print('PASS actual Windows equality + independent signed-integer geometry oracle; native baseline RED retained')
