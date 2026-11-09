#!/usr/bin/env python3
"""Compiler-owned include closure with the exact owning semantic flags; dependency output only."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
parser=argparse.ArgumentParser();parser.add_argument('--art',type=Path,required=True);args=parser.parse_args()
art=args.art.resolve();root=Path(__file__).resolve().parents[3];rows=[]
for row in json.loads((art/'mac-commands.json').read_text()):
    if row['phase']!='whole-owning-object':continue
    command=[];skip=False
    for argument in row['command']:
        if skip:skip=False;continue
        if argument in ['-o','-MF','-MT']:skip=True;continue
        if argument in ['-MD','-c']:continue
        command.append(argument)
    name=row['name'];dep=art/'owning-objects'/(name+'.d')
    command+=['-M','-MF',str(dep),'-MT',str(art/'owning-objects'/(name+'.o'))]
    result=subprocess.run(command,cwd=row['directory'],capture_output=True,text=True,timeout=60)
    (art/(name+'-closure.log')).write_text(result.stdout+result.stderr)
    assert result.returncode==0,(name,result.stderr)
    pieces=shlex.split(dep.read_text().replace('\\\n',''));assert pieces[0].endswith(':')
    closure={str(Path(value).resolve()):hashlib.sha256(Path(value).read_bytes()).hexdigest() for value in pieces[1:]}
    assert len(closure)>20,(name,len(closure))
    (art/(name+'-closure.json')).write_text(json.dumps(closure,indent=2,sort_keys=True)+'\n')
    rows.append({'context':name,'command':command,'exit':result.returncode,'headers_and_source':len(closure)})
    print('PASS actual compiler closure',name,len(closure))
(art/'owning-closure-commands.json').write_text(json.dumps(rows,indent=2)+'\n')
