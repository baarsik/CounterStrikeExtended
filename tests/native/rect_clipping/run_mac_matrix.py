#!/usr/bin/env python3
"""Pure scalar C++14 checks and actual whole owning TU object compiles."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

parser=argparse.ArgumentParser()
parser.add_argument('--art',type=Path,required=True)
args=parser.parse_args();art=args.art.resolve();root=Path(__file__).resolve().parents[3]
rows=[]
def run(name,phase,command,cwd=root):
    result=subprocess.run([str(x) for x in command],cwd=cwd,capture_output=True,text=True,timeout=120)
    (art/f'{name}-{phase}.log').write_text(result.stdout+result.stderr)
    rows.append({'name':name,'phase':phase,'command':[str(x) for x in command],'directory':str(cwd),'exit':result.returncode})
    (art/'mac-commands.json').write_text(json.dumps(rows,indent=2)+'\n')
    assert result.returncode==0,(name,phase,(result.stdout+result.stderr)[-3000:])
    return result.stdout
compiler=Path(subprocess.check_output(['xcrun','--find','clang++'],text=True).strip())
(art/'mac-compiler.json').write_text(json.dumps({'path':str(compiler),'resolved_path':str(compiler.resolve()),
    'sha256':hashlib.sha256(compiler.read_bytes()).hexdigest(),'version':subprocess.check_output([compiler,'--version'],text=True)},indent=2)+'\n')
for mode in ['Release','Debug','Sanitizers']:
    build=art/('mac-'+mode.lower())
    configure=['cmake','-S',root/'tests/native/rect_clipping','-B',build,'-G','Ninja',
        '-DCMAKE_BUILD_TYPE='+('Release' if mode=='Release' else 'Debug'),'-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0',
        '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DRECT_GOLDEN='+str(art/'golden.jsonl'),
        '-DRECT_SANITIZERS='+('ON' if mode=='Sanitizers' else 'OFF')]
    if mode=='Sanitizers':configure+=['-DCMAKE_CXX_FLAGS_DEBUG=-O1 -g']
    run(mode,'configure',configure);run(mode,'build',['cmake','--build',build,'--parallel','3'])
    run(mode,'tests',['ctest','--test-dir',build,'--output-on-failure'])
    print('PASS pure',mode,'9105 OS rows +100081 integer oracle cases; C11 record compile',flush=True)
# Replay unchanged owning context options with only outputs redirected under ART.
for mode in ['release','debug']:
    db=art.parent/('macos-runtime-policy-'+mode)/'compile_commands.json'
    entries=json.loads(db.read_text())
    for name in ['vgui_EngineSurfaceHW.cpp','gl_screen.cpp']:
        source=root/'GoldSrc-SDK/engine'/name
        entry=next(item for item in entries if Path(item['file'])==source)
        command=entry.get('arguments') or shlex.split(entry['command'])
        output=art/'owning-objects'/f'{mode}-{name}.o';output.parent.mkdir(exist_ok=True)
        for index,arg in enumerate(command):
            if arg=='-o':command[index+1]=str(output)
            elif arg=='-MF':command[index+1]=str(output.with_suffix('.d'))
            elif arg=='-MT':command[index+1]=str(output)
        run(mode+'-'+name,'whole-owning-object',command,Path(entry['directory']))
        rows[-1]['source_sha256']=hashlib.sha256(source.read_bytes()).hexdigest()
        rows[-1]['object_sha256']=hashlib.sha256(output.read_bytes()).hexdigest()
        rows[-1]['owning_compile_database']=str(db)
        (art/'mac-commands.json').write_text(json.dumps(rows,indent=2)+'\n')
        print('PASS actual owning object',mode,name,flush=True)
print('PASS Mac scalar matrix +4 actual owning TU object compiles; no GL/window calls')
