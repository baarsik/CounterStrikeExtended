import hashlib,json,pathlib,re,subprocess,sys
root=pathlib.Path(__file__).resolve().parents[3];out=pathlib.Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
header=root/'src/public/steam_provider.h';source=header.read_text()
# Binding-only symbols: no SDK function is invoked by this test.
names=re.findall(r'provider_detail::Bind\(module, "([^"]+)"',source)
provider=out/'binding_only.cpp';provider.write_text('#include <cstdlib>\n'+'\n'.join('extern "C" __attribute__((visibility("default"))) void '+name+'() { std::abort(); }' for name in names)+'\n')
flags=['clang++','-std=c++17',('-Os' if '--release' in sys.argv else '-O0'),'-UNDEBUG','-I'+str(root/'src')]
commands=[flags+['-dynamiclib',str(provider),'-o',str(out/'binding-only.dylib')],flags+(['-DCSE_UNCHECKED_PROVIDER'] if 'bool CloseChecked()' not in source else [])+[str(root/'tests/native/software_lifetime/provider_close.cpp'),'-o',str(out/'provider_close')],[str(out/'provider_close'),str(out/'binding-only.dylib')]]
rows=[]
for index,command in enumerate(commands):
    run=subprocess.run(command,capture_output=True);log=out/(str(index)+'.log');log.write_bytes(run.stdout+run.stderr);rows.append({'command':command,'exit':run.returncode,'log_sha256':hashlib.sha256(log.read_bytes()).hexdigest()});print((run.stdout+run.stderr).decode())
    if run.returncode:break
(out/'receipt.json').write_text(json.dumps({'source_sha256':hashlib.sha256(header.read_bytes()).hexdigest(),'commands':rows},indent=2)+'\n');sys.exit(run.returncode)
