#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,required=True);parser.add_argument('--headers',type=Path,required=True);parser.add_argument('--work',type=Path,required=True);parser.add_argument('--art',type=Path,required=True);args=parser.parse_args()
work=args.work.resolve();art=args.art.resolve();work.mkdir(parents=True,exist_ok=True);art.mkdir(parents=True,exist_ok=True);source=work/'source';shutil.copytree(args.source,source,dirs_exist_ok=True);sdk=args.headers/'src';rows=[]
cases=[('normal',value) for value in [1,1000,1000000,2000000,2000001,10000000,1000000000,4294967296,4294967296000000]]+[('seeded',1000000000),('same-time',1000000),('millis',1000000)]
for cpu in ['x86','x64','arm64']:
    setup=work/f'setup-{cpu}.cmd';setup.write_text('@echo off\ncall "C:\\BuildTools\\2026\\Common7\\Tools\\VsDevCmd.bat" -no_logo -host_arch=arm64 -arch='+cpu+' >nul\nif errorlevel 1 exit /b 1\nset\n')
    result=subprocess.run(['cmd.exe','/d','/c',setup],capture_output=True,text=True,check=True);env=dict(os.environ)
    for line in result.stdout.splitlines():
        if '=' in line and not line.startswith('='):
            key,value=line.split('=',1);env[key]=value
    compiler=Path(shutil.which('cl.exe',path=env.get('Path',env.get('PATH'))))
    (art/f'compiler-{cpu}.json').write_text(json.dumps({'path':str(compiler),'sha256':hashlib.sha256(compiler.read_bytes()).hexdigest(),'INCLUDE':env.get('INCLUDE',''),'CL':env.get('CL',''),'_CL_':env.get('_CL_','')},indent=2)+'\n')
    for mode in ['Release','Debug']:
        name=cpu+'-'+mode;build=work/name;build.mkdir(exist_ok=True)
        flags=['/nologo','/std:c++14','/EHsc','/fp:precise','/W4','/WX','/D_CRT_SECURE_NO_WARNINGS',
            '/I'+str(source/'imports'),'/I'+str(sdk/'vgui2/src/public'),'/I'+str(sdk/'public')]
        flags+=['/O2','/MD','/DNDEBUG'] if mode=='Release' else ['/Od','/Zi','/MDd','/D_DEBUG']
        exe=build/'system_clock_reference.exe';command=[str(compiler)]+flags+[str(source/'windows_reference.cpp'),'/Fe'+str(exe)]
        result=subprocess.run(command,cwd=build,env=env,capture_output=True,timeout=120);(art/(name+'-compile.log')).write_bytes(result.stdout+result.stderr);rows.append({'context':name,'phase':'compile','command':command,'exit':result.returncode});(art/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');assert result.returncode==0,rows[-1]
        shutil.copy2(exe,art/(name+'.exe'))
        for scenario,frequency in cases:
            command=[str(exe),scenario,str(frequency)];result=subprocess.run(command,cwd=build,env=env,capture_output=True,timeout=30)
            tag=name+'-'+scenario+'-'+str(frequency);(art/(tag+'.jsonl')).write_bytes(result.stdout);(art/(tag+'-observations.log')).write_bytes(result.stderr)
            rows.append({'context':name,'phase':'controlled-execute','scenario':scenario,'frequency':frequency,'command':command,'exit':result.returncode});(art/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');assert result.returncode==0,rows[-1]
            parsed=[json.loads(line) for line in result.stdout.splitlines()];assert parsed
        for scenario in ['constructor-throw','counter-throw']:
            command=[str(exe),scenario,'1000000'];result=subprocess.run(command,cwd=build,env=env,capture_output=True,timeout=30)
            tag=name+'-'+scenario;(art/(tag+'.failure.json')).write_bytes(result.stdout);(art/(tag+'-observations.log')).write_bytes(result.stderr)
            rows.append({'context':name,'phase':'controlled-exception','scenario':scenario,'command':command,'exit':result.returncode});(art/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');assert result.returncode==0,rows[-1]
        print('PASS',name,'actual declaration/constructor/timer +12 controlled cases +2 exception observations; no real clock calls',flush=True)
print('PASS 6 MSVC contexts/84 controlled reference cases; compare encoded results and FP observations separately')
