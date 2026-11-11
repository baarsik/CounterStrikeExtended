"""Owning metadata must fail closed for an absent terminal TU, wrong image or missing EH policy."""
import copy
import json
import pathlib
import sys
root=pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools'))
import translate_msbuild as translator
configs={mode:translator.evaluate(root/'GoldSrc-SDK/swds/swds.vcxproj',mode) for mode in ('Debug','Release')}
lines=[]
translator.emit_mac_software_lifetime(lines,'swds',configs)
assert lines[0]=='if(APPLE AND CMAKE_SIZEOF_VOID_P EQUAL 8)'
assert sum('CSE_TEXT_ENTRY_USE_PTHREAD_KEY=1' in line for line in lines)==2
for case in ('wrong-owner','missing-gate','no-sync','config-drift','wrong-platform'):
    altered=copy.deepcopy(configs);target='swds'
    if case=='wrong-owner':target='hw_engine'
    elif case=='config-drift':altered['Release']['properties']['CseMacSoftwareLifetime']=''
    else:
        for data in altered.values():
            item=next(item for item in data['items'] if item['path'].endswith('/engine/native_software_lifetime.cpp'))
            if case=='missing-gate':data['items'].remove(item)
            elif case=='no-sync':item['metadata']['ExceptionHandling']='false'
            else:item['metadata']['CseTargetOS']='linux'
    try:translator.emit_mac_software_lifetime([],target,altered)
    except ValueError:pass
    else:raise AssertionError('accepted '+case)
if len(sys.argv)>1:
    before=json.loads(pathlib.Path(sys.argv[1]).read_text())
    after={config:{'definitions':data['definitions'],'active_items':[item for item in data['items'] if item['metadata'].get('ExcludedFromBuild')!='true']} for config,data in configs.items()}
    assert after==before,'Reference Win32 effective metadata changed'
print('PASS exact two-owner Apple selection, five fail-closed negatives and retained Win32 policy')
