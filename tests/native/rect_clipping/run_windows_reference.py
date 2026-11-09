#!/usr/bin/env python3
"""Execute only imported pure scalar methods and documented User32 rectangle API."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--art', type=Path, required=True)
parser.add_argument('--work', type=Path, required=True)
args=parser.parse_args()
art=args.art.resolve();work=args.work.resolve()
work.mkdir(parents=True,exist_ok=True);art.mkdir(parents=True,exist_ok=True)
source=work/'source';shutil.copytree(args.source,source,dirs_exist_ok=True)
rows=[]
for cpu in ['x86','x64','arm64']:
    setup=work/f'setup-{cpu}.cmd'
    setup.write_text('@echo off\ncall "C:\\BuildTools\\2026\\Common7\\Tools\\VsDevCmd.bat" -no_logo -host_arch=arm64 -arch='+cpu+' >nul\nif errorlevel 1 exit /b 1\nset\n')
    env=dict(os.environ)
    result=subprocess.run(['cmd.exe','/d','/c',setup],capture_output=True,text=True,check=True)
    for line in result.stdout.splitlines():
        if '=' in line and not line.startswith('='):
            key,value=line.split('=',1);env[key]=value
    compiler=Path(shutil.which('cl.exe',path=env.get('Path',env.get('PATH'))))
    (art/f'compiler-{cpu}.json').write_text(json.dumps({'path':str(compiler),'sha256':hashlib.sha256(compiler.read_bytes()).hexdigest(),
        'INCLUDE':env.get('INCLUDE',''),'CL':env.get('CL',''),'_CL_':env.get('_CL_','')},indent=2)+'\n')
    for mode in ['Release','Debug']:
        for model in ['windows-reference','portable-baseline']:
            name=f'{cpu}-{mode}-{model}';build=work/name;build.mkdir(exist_ok=True)
            # Actual owning target: per-file /fp:precise, /O1 or /Od, x86 SSE2, DLL CRT; no new EH override.
            flags=['/nologo','/std:c++17','/W4','/WX','/fp:precise','/D_MBCS']
            flags+=['/O1','/Gy','/MD','/DNDEBUG'] if mode=='Release' else ['/Od','/Zi','/MDd','/D_DEBUG']
            if cpu=='x86':flags+=['/arch:SSE2']
            exe=build/'rect_rows.exe'
            command=[str(compiler)]+flags+['/I'+str(source/'headers')]+[str(source/'reference_rows.cpp'),str(source/model/'imports/imported_surface.cpp'),
                str(source/model/'imports/imported_screen.cpp'),
                '/Fe'+str(exe),'/link','user32.lib']
            result=subprocess.run(command,cwd=build,env=env,capture_output=True,timeout=120)
            (art/f'{name}-compile.log').write_bytes(result.stdout+result.stderr)
            rows.append({'name':name,'phase':'compile','command':command,'exit':result.returncode})
            (art/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');assert result.returncode==0,rows[-1]
            result=subprocess.run([exe],cwd=build,env=env,capture_output=True,timeout=30)
            (art/f'{name}.jsonl').write_bytes(result.stdout)
            (art/f'{name}-stderr.log').write_bytes(result.stderr)
            rows.append({'name':name,'phase':'pure-execute','command':[str(exe)],'exit':result.returncode})
            (art/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');assert result.returncode==0,rows[-1]
            parsed=[json.loads(line) for line in result.stdout.splitlines()];assert len(parsed)==9105,len(parsed)
            shutil.copy2(exe,art/f'{name}.exe')
            print('PASS',name,len(parsed),'rows',flush=True)
print('PASS 6 actual Windows API profiles and 6 controlled legacy32 native-body baseline profiles')
