#!/usr/bin/env python3
"""Exact method imports with explicitly controlled bindings, not a full CInput ABI fixture."""
import argparse
import hashlib
import json
from pathlib import Path

def function(text, signature):
    begin=text.index(signature);brace=text.index('{',begin);depth=1;index=brace+1
    while depth:
        depth+=(text[index]=='{')-(text[index]=='}');index+=1
    return text[begin:index]
parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
source=args.source.resolve();text=source.read_bytes().decode('latin1').replace('\r\n','\n');out=args.output;out.mkdir(parents=True,exist_ok=True)
helper=text[text.index('namespace {\nusing cse_vgui2_input::VirtualKey;'):text.index('\nstruct ToggleButton_t')]
helper=helper[:helper.rindex('#endif')]
table=text[text.index('struct ToggleButton_t'):text.index('\nclass CInputWin32')]
methods={name:function(text,'void CInputWin32::'+name+'(') for name in ['SetCursorPos','GetCursorPos','UpdateToggleButtonState','GetKeyCodeText']}
constructor=function(text,'CInputWin32::CInputWin32()');assignments=constructor[constructor.index('\t// build key'):constructor.rindex('}')]
(out/'native_boundary.inc').write_text(helper)
(out/'toggle_table.inc').write_text(table)
(out/'methods.inc').write_text('\n\n'.join(methods.values())+'\n')
(out/'translation.inc').write_text(assignments)
(out/'imports.json').write_text(json.dumps({'source':str(source),'sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
 'native_helper':hashlib.sha256(helper.encode()).hexdigest(),'table':hashlib.sha256(table.encode()).hexdigest(),
 'methods':{key:hashlib.sha256(value.encode()).hexdigest() for key,value in methods.items()},
 'bindings':'Controlled method-state/cursor callbacks; not class layout, full focus, IME or provider acceptance'},indent=2)+'\n')
