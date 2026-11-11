import hashlib,json,pathlib,subprocess,sys
root=pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tests/native/server_browser_ui'))
from extract_owners import extract
out=pathlib.Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
owner=root/'GoldSrc-SDK/engine/sys_dll2.cpp';body,_=extract(owner.read_text(),'cse::software_lifetime::RetireResources',0,1)
(out/'terminal_adapter.inc').write_text(body)
command=['clang++','-std=c++17',('-Os' if '--release' in sys.argv else '-O0'),'-UNDEBUG','-I'+str(out),'-I'+str(root/'GoldSrc-SDK'),str(root/'tests/native/software_lifetime/terminal_adapter.cpp'),str(root/'GoldSrc-SDK/engine/native_software_lifetime.cpp'),'-o',str(out/'terminal_adapter')]
run=subprocess.run(command,capture_output=True);(out/'compile.log').write_bytes(run.stdout+run.stderr)
receipt={'source_sha256':hashlib.sha256(owner.read_bytes()).hexdigest(),'body_sha256':hashlib.sha256(body.encode()).hexdigest(),'command':command,'compile_exit':run.returncode,'runs':[]}
if not run.returncode:
    for mode in ['client','client-run','dedicated']:
        run=subprocess.run([str(out/'terminal_adapter'),mode],capture_output=True);receipt['runs'].append({'mode':mode,'exit':run.returncode});(out/(mode+'.log')).write_bytes(run.stdout+run.stderr);print((run.stdout+run.stderr).decode())
        if run.returncode: break
else: print((run.stdout+run.stderr).decode())
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');sys.exit(run.returncode)
