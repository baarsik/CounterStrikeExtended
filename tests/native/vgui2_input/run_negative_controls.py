#!/usr/bin/env python3
import argparse
import json
from pathlib import Path
import subprocess
parser=argparse.ArgumentParser();parser.add_argument('--art',type=Path,required=True);args=parser.parse_args()
root=Path(__file__).resolve().parents[3];art=args.art.resolve();out=art/'negative-controls';out.mkdir(exist_ok=True);rows=[]
def run(name,phase,command,expected):
    result=subprocess.run([str(x) for x in command],cwd=root,capture_output=True,text=True,timeout=120)
    (out/f'{name}-{phase}.log').write_text(result.stdout+result.stderr);rows.append({'name':name,'phase':phase,'command':[str(x) for x in command],'exit':result.returncode});(out/'commands.json').write_text(json.dumps(rows,indent=2)+'\n')
    assert (result.returncode==0)==expected,(name,phase,result.stdout,result.stderr)
source=(root/'src/vgui2/src/InputWin32.cpp').read_bytes().decode('latin1').replace('\r\n','\n')
controls=[
 ('context-guard','if ( m_hContext != DEFAULT_INPUT_CONTEXT )','if ( false && m_hContext != DEFAULT_INPUT_CONTEXT )'),
 ('ignore-rule','if ( ignore != KEY_NONE && IsKeyDown( ignore ) )','if ( ignore != KEY_NONE )'),
 ('snapshot-bound','count <= static_cast<int>(scancode)','count < static_cast<int>(scancode)'),
 ('snapshot-before-guards','void CInputWin32::UpdateToggleButtonState()\n{',
  'void CInputWin32::UpdateToggleButtonState()\n{\n#if !defined(_WIN32)\n    (void)CaptureNativeKeyState();\n#endif'),
 ('cursor-cannot-noop','NativeInputFailure("SurfaceSetCursorPos requires native provider cursor functions");','return;'),
]
for name,before,after in controls:
    directory=out/name;directory.mkdir(exist_ok=True);assert source.count(before)==1,(name,source.count(before))
    path=directory/'InputWin32.cpp';path.write_bytes(source.replace(before,after,1).replace('\n','\r\n').encode('latin1'));build=directory/'build'
    run(name,'configure',['cmake','-S',root/'tests/native/vgui2_input','-B',build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',
        '-DINPUT_SOURCE='+str(path),'-DINPUT_GOLDEN='+str(art/'windows/golden.jsonl')],True)
    run(name,'build',['cmake','--build',build,'--parallel','3'],True)
    run(name,'tests',['ctest','--test-dir',build,'--output-on-failure'],False)
    print('PASS rejected',name,flush=True)
header=(root/'src/vgui2/src/NativeKeyState.h').read_text()
for name,before,after in [('down-is-not-bool','DownMask = 0x8000','DownMask = 0x0100'),
    ('unknown-is-not-success','default:\n        return false;','default:\n        return true;')]:
    directory=out/name;directory.mkdir(exist_ok=True);assert header.count(before)==1
    (directory/'NativeKeyState.h').write_text(header.replace(before,after,1));exe=directory/'key_states'
    run(name,'compile',['clang++','-std=c++14','-Wall','-Wextra','-Werror','-I'+str(directory),root/'tests/native/vgui2_input/key_state_tests.cpp','-o',exe],True)
    run(name,'execute',[exe],False);print('PASS rejected',name,flush=True)
print('PASS all 7 native key/cursor/ordering/unknown-state mutations reject')
