#!/usr/bin/env python3
"""C/C++ real common-port/canonical include orders on the Apple host and LINUX branch."""
import argparse
import json
from pathlib import Path
import subprocess

parser=argparse.ArgumentParser();parser.add_argument('--art',type=Path,required=True);args=parser.parse_args()
root=Path(__file__).resolve().parents[3];art=args.art.resolve();out=art/'include-orders';out.mkdir(exist_ok=True);rows=[]
for platform,flags in [('apple',['-DOSX']),('linux-definition-on-apple-host',['-DLINUX','-DOSX'])]:
    for language,compiler,standard,suffix in [('C','clang','c11','c'),('C++','clang++','c++14','cpp')]:
        for order in ['canonical-first','port-first']:
            includes=['cse_rect.h','port.h'] if order=='canonical-first' else ['port.h','cse_rect.h']
            name=f'{platform}-{language.replace("+","p")}-{order}'
            source=out/(name+'.'+suffix)
            assertion='_Static_assert' if language=='C' else 'static_assert'
            source.write_text(''.join('#include "'+header+'"\n' for header in includes)+'#include <stddef.h>\n'
                +assertion+'(sizeof(RECT)==16 && offsetof(RECT,top)==4 && offsetof(RECT,right)==8,"canonical record");\n')
            command=[compiler,'-std='+standard,'-Wall','-Wextra','-Werror','-fsyntax-only']+flags+[
                '-I'+str(root/'src/common'),'-I'+str(root/'src/public'),str(source)]
            result=subprocess.run(command,cwd=root,capture_output=True,text=True,timeout=20)
            (out/(name+'.log')).write_text(result.stdout+result.stderr)
            rows.append({'platform_definition':platform,'language':language,'order':order,'command':command,'exit':result.returncode})
            (art/'include-order-commands.json').write_text(json.dumps(rows,indent=2)+'\n')
            assert result.returncode==0,(name,result.stdout,result.stderr)
print('PASS 8 actual C11/C++14 include orders; LINUX branch is host-defined, not Linux runtime qualification')
