import hashlib,json,pathlib,subprocess,sys
root=pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tests/native/server_browser_ui'))
from extract_owners import closing_brace, extract
out=pathlib.Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
owner=root/'GoldSrc-SDK/engine/native_runtime_bootstrap.cpp';source=owner.read_text();bodies=[]
for name in ['CseNativeRuntimeOpenV1','CseNativeRuntimeCloseV1']:
    start=source.index('int '+name);bodies.append(source[start:closing_brace(source,source.index('{',start))])
body='\n'.join(bodies);(out/'bootstrap.inc').write_text(body)
game_owner=root/'GoldSrc-SDK/engine/sys_engine.cpp'
game_source=game_owner.read_text()
game_body='\n'.join(extract(game_source,name,0,1)[0] for name in ['cse::InitializeNativeGameUIHost','cse::NativeGameUIIsMainThread','cse::NativeGameUIIsRetirementSafePoint','cse::EnterNativeGameUIUse','cse::LeaveNativeGameUIUse','cse::EnterNativeGameUIRetirement','cse::LeaveNativeGameUIRetirement'])
(out/'game_identity.inc').write_text(game_body)
command=['clang++','-std=c++17',('-Os' if '--release' in sys.argv else '-O0'),'-UNDEBUG','-DSW_RENDERER','-Wall','-Wextra','-Werror','-I'+str(out),'-I'+str(root/'GoldSrc-SDK'),str(root/'tests/native/software_lifetime/bootstrap.cpp'),str(root/'GoldSrc-SDK/engine/native_software_lifetime.cpp'),'-o',str(out/'bootstrap')]
run=subprocess.run(command,capture_output=True);(out/'compile.log').write_bytes(run.stdout+run.stderr)
receipt={'source_sha256':hashlib.sha256(owner.read_bytes()).hexdigest(),'game_identity_source_sha256':hashlib.sha256(game_owner.read_bytes()).hexdigest(),'game_identity_body_sha256':hashlib.sha256(game_body.encode()).hexdigest(),'body_sha256':hashlib.sha256(body.encode()).hexdigest(),'command':command,'compile_exit':run.returncode}
if not run.returncode:
    run=subprocess.run([str(out/'bootstrap')],capture_output=True);receipt['run_exit']=run.returncode;(out/'run.log').write_bytes(run.stdout+run.stderr)
print((run.stdout+run.stderr).decode());(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');sys.exit(run.returncode)
