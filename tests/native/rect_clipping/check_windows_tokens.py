#!/usr/bin/env python3
"""Preprocess real whole TUs against the frozen owner header tree; compare selected tokens."""
import argparse
import hashlib
import json
import os
from pathlib import Path
from cpp_tokens import tokens
import shutil
import subprocess

parser=argparse.ArgumentParser()
parser.add_argument('--before',type=Path,required=True)
parser.add_argument('--after',type=Path,required=True)
parser.add_argument('--headers',type=Path,required=True)
parser.add_argument('--art',type=Path,required=True)
parser.add_argument('--work',type=Path,required=True)
args=parser.parse_args();art=args.art.resolve();work=args.work.resolve();sdk=args.headers/'src'
art.mkdir(parents=True,exist_ok=True);work.mkdir(parents=True,exist_ok=True)
rows=[]
for cpu in ['x86','x64','arm64']:
    setup=work/f'setup-{cpu}.cmd';setup.write_text('@echo off\ncall "C:\\BuildTools\\2026\\Common7\\Tools\\VsDevCmd.bat" -no_logo -host_arch=arm64 -arch='+cpu+' >nul\nif errorlevel 1 exit /b 1\nset\n')
    result=subprocess.run(['cmd.exe','/d','/c',setup],capture_output=True,text=True,check=True);env=dict(os.environ)
    for line in result.stdout.splitlines():
        if '=' in line and not line.startswith('='):
            key,value=line.split('=',1);env[key]=value
    compiler=Path(shutil.which('cl.exe',path=env.get('Path',env.get('PATH'))))
    for mode in ['Release','Debug']:
        flags=['/nologo','/TP','/EP','/std:c++17','/W4','/WX','/fp:precise','/DWIN32','/DCSE_ENGINE_WSOCK32=1',
            '/DHL25_HW_ENGINE','/D_CRT_SECURE_NO_WARNINGS','/DLINK_MPG123_DLL','/D_MBCS']
        flags+=['/O1','/MD','/DNDEBUG'] if mode=='Release' else ['/Od','/MDd','/D_DEBUG']
        if cpu=='x86':flags+=['/arch:SSE2']
        includes=[sdk/'engine',sdk/'external/installed/x86-windows-static-md/include',sdk/'external/installed/x86-windows-static-md/include/webm',
            sdk/'common',sdk/'cl_dll',sdk/'public',sdk/'pm_shared',sdk/'utils/vgui/include',sdk/'external/SDL3/include']
        flags+=['/I'+str(path) for path in includes]
        for rel in ['src/engine/vgui_EngineSurfaceHW.cpp','src/engine/gl_screen.cpp','src/common/port.h']:
            # Identical primary filename for both runs preserves actual __FILE__ tokens.
            primary=work/'selected'/rel;primary.parent.mkdir(parents=True,exist_ok=True)
            selected={}
            for side in ['before','after']:
                source=getattr(args,side)/rel;shutil.copy2(source,primary)
                command=[str(compiler)]+flags+[str(primary)]
                result=subprocess.run(command,env=env,cwd=work,capture_output=True,timeout=120)
                name=f'{cpu}-{mode}-{Path(rel).stem}-{side}'
                (art/(name+'.i')).write_bytes(result.stdout);(art/(name+'.stderr')).write_bytes(result.stderr)
                rows.append({'context':cpu+'-'+mode,'source':rel,'side':side,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
                    'command':command,'exit':result.returncode})
                (art/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');assert result.returncode==0,rows[-1]
                selected[side]=tokens(result.stdout)
            assert selected['before']==selected['after'],(cpu,mode,rel,'Windows selected code changed')
            rows[-1]['selected_equal']=True;rows[-1]['tokens']=len(selected['after'])
            rows[-1]['selected_sha256']=hashlib.sha256(b'\0'.join(selected['after'])).hexdigest()
            (art/'commands.json').write_text(json.dumps(rows,indent=2)+'\n')
            print('PASS selected',cpu,mode,rel,len(selected['after']),flush=True)
            if cpu=='x86' and mode=='Release' and rel.endswith('vgui_EngineSurfaceHW.cpp'):
                text=primary.read_text()
                before='const ULONG difference = static_cast<ULONG>( position ) - static_cast<ULONG>( origin );'
                assert text.count(before)==1
                primary.write_text(text.replace(before,before.replace('origin );','origin ) + 1;'),1))
                result=subprocess.run(command,env=env,cwd=work,capture_output=True,timeout=120)
                (art/'CONTROL-selected-coordinate-plus1.i').write_bytes(result.stdout)
                assert result.returncode==0 and tokens(result.stdout)!=selected['after'], 'selected edit escaped'
                rows.append({'control':'coordinate-plus1','exit':result.returncode,'rejected':True,'command':command})
                (art/'commands.json').write_text(json.dumps(rows,indent=2)+'\n')
print('PASS 18 whole-TU/header selected-token equalities, no runtime calls')
