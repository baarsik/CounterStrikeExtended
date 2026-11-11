"""Production image gate + executable anchor + private registry candidate; no external provider sessions."""
import hashlib,json,pathlib,subprocess,sys
root=pathlib.Path(__file__).resolve().parents[3];out=pathlib.Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
base=['clang++','-std=c++17',('-Os' if '--release' in sys.argv else '-O0'),'-UNDEBUG','-Wall','-Wextra','-Werror','-I'+str(root/'GoldSrc-SDK')]
receipts=[]
def run(args,name):
    result=subprocess.run(args,capture_output=True);log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
    receipts.append({'command':args,'exit':result.returncode,'log_sha256':hashlib.sha256(log.read_bytes()).hexdigest()});(out/'receipt.json').write_text(json.dumps(receipts,indent=2)+'\n')
    print(name,result.returncode,flush=True)
    if result.returncode:print((result.stdout+result.stderr).decode());raise SystemExit(result.returncode)
for name in ['gate_host','image_owner']:
    run(base+[str(root/'tests/native/software_lifetime'/f'{name}.cpp'),'-o',str(out/name)],name+'-compile')
for variant in ['unselected','candidate']:
    flags=[]
    if variant=='candidate': flags=['-DCSE_TEXT_ENTRY_USE_PTHREAD_KEY=1',str(root/'GoldSrc-SDK/common/TextEntryClipboard.cpp')]
    image=out/(variant+'.dylib')
    run(base+['-dynamiclib','-fvisibility=hidden']+flags+[str(root/'GoldSrc-SDK/engine/native_software_lifetime.cpp'),str(root/'tests/native/software_lifetime/gate_provider.cpp'),'-o',str(image)],variant+'-compile')
    for name in ['gate_host','image_owner']:run([str(out/name),str(image)],variant+'-'+name)

run(base+[str(root/'tests/native/software_lifetime/private_admission.cpp'),'-o',str(out/'private_admission')],'private-admission-compile')
for mismatch in range(1,5):
    image=out/('bad-contract-'+str(mismatch)+'.dylib')
    run(base+['-Wno-unused-function','-dynamiclib','-DCSE_BAD_CONTRACT='+str(mismatch),str(root/'tests/native/software_lifetime/private_provider.cpp'),'-o',str(image)],'bad-contract-'+str(mismatch)+'-compile')
    run([str(out/'private_admission'),str(image)],'bad-contract-'+str(mismatch))
