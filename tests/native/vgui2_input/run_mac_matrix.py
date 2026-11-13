#!/usr/bin/env python3
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
parser=argparse.ArgumentParser();parser.add_argument('--art',type=Path,required=True);args=parser.parse_args()
root=Path(__file__).resolve().parents[3];art=args.art.resolve();rows=[]
def run(name,phase,command,cwd=root):
    result=subprocess.run([str(x) for x in command],cwd=cwd,capture_output=True,text=True,timeout=120)
    (art/f'{name}-{phase}.log').write_text(result.stdout+result.stderr)
    rows.append({'name':name,'phase':phase,'command':[str(x) for x in command],'directory':str(cwd),'exit':result.returncode})
    (art/'mac-commands.json').write_text(json.dumps(rows,indent=2)+'\n');assert result.returncode==0,(name,phase,(result.stdout+result.stderr)[-2500:])
compiler=Path(subprocess.check_output(['xcrun','--find','clang++'],text=True).strip())
(art/'mac-compiler.json').write_text(json.dumps({'path':str(compiler),'resolved_path':str(compiler.resolve()),
    'sha256':hashlib.sha256(compiler.read_bytes()).hexdigest(),'version':subprocess.check_output([compiler,'--version'],text=True)},indent=2)+'\n')
for mode in ['Release','Debug','Sanitizers']:
    build=art/('mac-'+mode.lower())
    command=['cmake','-S',root/'tests/native/vgui2_input','-B',build,'-G','Ninja',
        '-DCMAKE_BUILD_TYPE='+('Release' if mode=='Release' else 'Debug'),'-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
        '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0','-DINPUT_GOLDEN='+str(art/'windows/golden.jsonl'),
        '-DINPUT_SANITIZERS='+('ON' if mode=='Sanitizers' else 'OFF')]
    if mode=='Sanitizers':command+=['-DCMAKE_CXX_FLAGS_DEBUG=-O1 -g']
    run(mode,'configure',command);run(mode,'build',['cmake','--build',build,'--parallel','3']);run(mode,'tests',['ctest','--test-dir',build,'--output-on-failure'])
    print('PASS Mac',mode,'6144 exact method rows, exhaustive protocol and4 expected failure children',flush=True)
for mode in ['release','debug']:
    database=art.parent/('macos-runtime-policy-'+mode)/'compile_commands.json'
    entry=next(item for item in json.loads(database.read_text()) if Path(item['file'])==root/'src/vgui2/src/InputWin32.cpp')
    command=entry.get('arguments') or shlex.split(entry['command']);output=art/(mode+'-InputWin32.o')
    for index,arg in enumerate(command):
        if arg=='-o':command[index+1]=str(output)
    command+=['-MD','-MF',str(output.with_suffix('.d'))]
    run(mode,'whole-owning-object',command,Path(entry['directory']))
    rows[-1]['source_sha256']=hashlib.sha256(Path(entry['file']).read_bytes()).hexdigest()
    rows[-1]['object_sha256']=hashlib.sha256(output.read_bytes()).hexdigest()
    rows[-1]['database']=str(database)
    (art/'mac-commands.json').write_text(json.dumps(rows,indent=2)+'\n')
    closure={str(Path(value).resolve()):hashlib.sha256(Path(value).read_bytes()).hexdigest()
        for value in shlex.split(output.with_suffix('.d').read_text().replace('\\\n',''))[1:]}
    assert len(closure)>50
    (art/(mode+'-closure.json')).write_text(json.dumps(closure,indent=2,sort_keys=True)+'\n')
    print('PASS actual owning object',mode,'retained NULL conversion warnings remain in log',flush=True)
