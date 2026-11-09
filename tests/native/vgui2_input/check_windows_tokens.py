#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
from cpp_tokens import tokens
parser=argparse.ArgumentParser();parser.add_argument('--before',type=Path,required=True);parser.add_argument('--after',type=Path,required=True);parser.add_argument('--headers',type=Path,required=True);parser.add_argument('--art',type=Path,required=True);parser.add_argument('--work',type=Path,required=True);args=parser.parse_args()
art=args.art.resolve();work=args.work.resolve();sdk=args.headers/'GoldSrc-SDK';art.mkdir(parents=True,exist_ok=True);work.mkdir(parents=True,exist_ok=True);rows=[]
for cpu in ['x86','x64','arm64']:
    setup=work/f'setup-{cpu}.cmd';setup.write_text('@echo off\ncall "C:\\BuildTools\\2026\\Common7\\Tools\\VsDevCmd.bat" -no_logo -host_arch=arm64 -arch='+cpu+' >nul\nif errorlevel 1 exit /b 1\nset\n')
    result=subprocess.run(['cmd.exe','/d','/c',setup],capture_output=True,text=True,check=True);env=dict(os.environ)
    for line in result.stdout.splitlines():
        if '=' in line and not line.startswith('='):
            key,value=line.split('=',1);env[key]=value
    compiler=Path(shutil.which('cl.exe',path=env.get('Path',env.get('PATH'))))
    for mode in ['Release','Debug']:
        name=cpu+'-'+mode
        flags=['/nologo','/std:c++14','/EHsc','/fp:precise','/W4','/WX','/EP','/DWIN32','/D_WINDOWS','/D_USRDLL',
            '/D_CRT_SECURE_NO_WARNINGS','/D_CRT_NONSTDC_NO_DEPRECATE','/D_MBCS']
        flags+=['/O2','/MD','/DNDEBUG'] if mode=='Release' else ['/Od','/MDd','/D_DEBUG']
        flags+=['/I'+str(path) for path in [sdk/'vgui2/src',sdk/'vgui2/src/public',sdk/'vgui2/src/vgui_surfacelib',sdk/'public']]
        primary=work/'InputWin32.cpp';selected={}
        for side in ['before','after']:
            source=getattr(args,side);shutil.copy2(source,primary);command=[str(compiler)]+flags+[str(primary)]
            result=subprocess.run(command,env=env,cwd=work,capture_output=True,timeout=120)
            (art/(name+'-'+side+'.i')).write_bytes(result.stdout);(art/(name+'-'+side+'.stderr')).write_bytes(result.stderr)
            rows.append({'context':name,'side':side,'command':command,'exit':result.returncode,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest()})
            (art/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');assert result.returncode==0,rows[-1]
            selected[side]=tokens(result.stdout)
        assert selected['before']==selected['after'],(name,'Windows source changed')
        rows[-1]['equal']=True;rows[-1]['tokens']=len(selected['after']);rows[-1]['sha256']=hashlib.sha256(b'\0'.join(selected['after'])).hexdigest()
        (art/'commands.json').write_text(json.dumps(rows,indent=2)+'\n')
        print('PASS Windows selected',name,len(selected['after']),'tokens',flush=True)
        if cpu=='x86' and mode=='Release':
            text=primary.read_bytes();original=b'_updateToggleButtonState = false;';assert text.count(original)==1
            primary.write_bytes(text.replace(original,b'_updateToggleButtonState = true;',1))
            result=subprocess.run(command,env=env,cwd=work,capture_output=True,timeout=120);(art/'CONTROL-wrong-frame-guard.i').write_bytes(result.stdout)
            assert result.returncode==0 and tokens(result.stdout)!=selected['after'],'selected guard mutation escaped'
            rows.append({'control':'wrong-frame-guard','rejected':True,'exit':result.returncode,'command':command})
            (art/'commands.json').write_text(json.dumps(rows,indent=2)+'\n')
print('PASS 6 full-TU Windows selected-token equalities and rejecting guard mutation')
