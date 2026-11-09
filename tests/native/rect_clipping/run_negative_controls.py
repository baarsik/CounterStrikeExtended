#!/usr/bin/env python3
"""Controlled copies only: restore actual RED or break a measured representation/math fact."""
import argparse
import json
from pathlib import Path
import subprocess

parser=argparse.ArgumentParser();parser.add_argument('--art',type=Path,required=True);args=parser.parse_args()
root=Path(__file__).resolve().parents[3];art=args.art.resolve();out=art/'negative-controls';out.mkdir(exist_ok=True);rows=[]
def run(name,phase,command,success):
    result=subprocess.run([str(x) for x in command],cwd=root,capture_output=True,text=True,timeout=120)
    (out/f'{name}-{phase}.log').write_text(result.stdout+result.stderr)
    rows.append({'name':name,'phase':phase,'command':[str(x) for x in command],'exit':result.returncode})
    (out/'commands.json').write_text(json.dumps(rows,indent=2)+'\n')
    assert (result.returncode==0)==success,(name,phase,result.stdout,result.stderr)
    return result
source=(root/'GoldSrc-SDK/engine/vgui_EngineSurfaceHW.cpp').read_text()
for name,before,after in [
    ('nonpositive-must-be-empty','rectangle->left >= rectangle->right || rectangle->top >= rectangle->bottom',
        'rectangle->left == rectangle->right && rectangle->top == rectangle->bottom'),
    ('uv-operation-order','/ width ) * ( s1 - s0 ) + s0;', '/ width ) * ( s0 - s1 ) + s0;')]:
    directory=out/name;directory.mkdir(exist_ok=True)
    assert before in source
    (directory/'vgui_EngineSurfaceHW.cpp').write_text(source.replace(before,after,1))
    (directory/'gl_screen.cpp').write_bytes((root/'GoldSrc-SDK/engine/gl_screen.cpp').read_bytes())
    build=directory/'build'
    run(name,'configure',['cmake','-S',root/'tests/native/rect_clipping','-B',build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',
        '-DRECT_SOURCE='+str(directory/'vgui_EngineSurfaceHW.cpp'),'-DRECT_GOLDEN='+str(art/'golden.jsonl')],True)
    run(name,'build',['cmake','--build',build,'--parallel','3'],True)
    run(name,'test',['ctest','--test-dir',build,'--output-on-failure'],False)
    print('PASS rejected',name,flush=True)
header=(root/'GoldSrc-SDK/common/cse_rect.h').read_text()
for name,before,after,compiles in [
    ('signed-mapping','return -1 - static_cast<std::int32_t>(UnsignedMaximum - bits);',
        'return static_cast<std::int32_t>(UnsignedMaximum - bits);',True),
    ('field-order','int32_t top;\n    int32_t right;','int32_t right;\n    int32_t top;',False)]:
    directory=out/name;directory.mkdir(exist_ok=True);assert header.count(before)==1
    (directory/'cse_rect.h').write_text(header.replace(before,after,1));exe=directory/'rect_types'
    result=run(name,'compile',['clang++','-std=c++14','-Wall','-Wextra','-Werror','-I'+str(directory),
        root/'tests/native/rect_clipping/type_arithmetic_tests.cpp','-o',exe],compiles)
    if compiles:
        run(name,'execute',[exe],False)
    else:
        assert 'static assertion failed' in result.stderr,result.stderr
    print('PASS rejected',name,flush=True)
print('PASS all 4 representation/arithmetic/actual-body mutation controls reject')
