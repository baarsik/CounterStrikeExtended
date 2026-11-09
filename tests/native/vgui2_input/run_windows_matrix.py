#!/usr/bin/env python3
"""Real SDK/domain + selected method reference with synthetic packed words; no host keyboard/cursor calls."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,required=True);parser.add_argument('--headers',type=Path,required=True);parser.add_argument('--work',type=Path,required=True);parser.add_argument('--art',type=Path,required=True);args=parser.parse_args()
work=args.work.resolve();art=args.art.resolve();work.mkdir(parents=True,exist_ok=True);art.mkdir(parents=True,exist_ok=True)
source=work/'source';shutil.copytree(args.source,source,dirs_exist_ok=True);sdk=args.headers/'GoldSrc-SDK';rows=[]
for cpu in ['x86','x64','arm64']:
    setup=work/f'setup-{cpu}.cmd';setup.write_text('@echo off\ncall "C:\\BuildTools\\2026\\Common7\\Tools\\VsDevCmd.bat" -no_logo -host_arch=arm64 -arch='+cpu+' >nul\nif errorlevel 1 exit /b 1\nset\n')
    result=subprocess.run(['cmd.exe','/d','/c',setup],capture_output=True,text=True,check=True);env=dict(os.environ)
    for line in result.stdout.splitlines():
        if '=' in line and not line.startswith('='):
            key,value=line.split('=',1);env[key]=value
    compiler=Path(shutil.which('cl.exe',path=env.get('Path',env.get('PATH'))))
    (art/f'compiler-{cpu}.json').write_text(json.dumps({'path':str(compiler),'sha256':hashlib.sha256(compiler.read_bytes()).hexdigest(),
        'INCLUDE':env.get('INCLUDE',''),'CL':env.get('CL',''),'_CL_':env.get('_CL_','')},indent=2)+'\n')
    for mode in ['Release','Debug']:
        name=cpu+'-'+mode;build=work/name;build.mkdir(exist_ok=True)
        flags=['/nologo','/std:c++14','/EHsc','/fp:precise','/W4','/WX','/D_CRT_SECURE_NO_WARNINGS','/DNOMINMAX']
        flags+=['/O2','/MD','/DNDEBUG'] if mode=='Release' else ['/Od','/Zi','/MDd','/D_DEBUG']
        flags+=['/I'+str(source),'/I'+str(source/'imports'),'/I'+str(sdk/'vgui2/src/public'),'/I'+str(sdk/'public')]
        for target,files in [('input_methods',['method_bindings.cpp','unrelated_callbacks.cpp','windows_sdk_probe.cpp']),('key_states',['key_state_tests.cpp'])]:
            exe=build/(target+'.exe');command=[str(compiler)]+flags+[str(source/file) for file in files]+['/Fe'+str(exe),'/link','user32.lib']
            result=subprocess.run(command,env=env,cwd=build,capture_output=True,timeout=120)
            (art/f'{name}-{target}-compile.log').write_bytes(result.stdout+result.stderr);rows.append({'context':name,'target':target,'phase':'compile','command':command,'exit':result.returncode});(art/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');assert result.returncode==0,rows[-1]
            result=subprocess.run([exe],env=env,cwd=build,capture_output=True,timeout=30)
            destination=art/(name+('-rows.jsonl' if target=='input_methods' else '-key-states.log'));destination.write_bytes(result.stdout+result.stderr)
            rows.append({'context':name,'target':target,'phase':'controlled-execute','command':[str(exe)],'exit':result.returncode});(art/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');assert result.returncode==0,rows[-1]
            if target=='input_methods':assert len([json.loads(line) for line in result.stdout.splitlines()])==6144
            shutil.copy2(exe,art/(name+'-'+target+'.exe'))
        print('PASS',name,'SDK assertions +6144 actual-method rows +packed-state exhaustive oracle',flush=True)
paths=sorted(art.glob('*-rows.jsonl'));assert len(paths)==6
golden=[json.loads(line) for line in paths[0].read_text().splitlines()]
for path in paths[1:]:assert [json.loads(line) for line in path.read_text().splitlines()]==golden,path
(art/'golden.jsonl').write_text(''.join(json.dumps(row,separators=(',',':'))+'\n' for row in golden))
print('PASS all 6 Windows references equal; no GetKeyState/SDL/cursor OS calls executed')
