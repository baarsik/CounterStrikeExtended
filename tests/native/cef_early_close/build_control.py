"""Build one private lifetime control from a frozen host source and existing flags."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess


def replace_once(source, old, new):
    assert source.count(old) == 1, old
    return source.replace(old, new, 1)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--packet', type=Path, required=True)
    parser.add_argument('--control', choices=['trace', 'retain-parent', 'post-close', 'both', 'candidate'], required=True)
    parser.add_argument('--configuration', choices=['Debug', 'Release'], default='Debug')
    args = parser.parse_args()
    name = args.control if args.configuration == 'Debug' else args.control + '-release'
    work = args.packet / name
    work.mkdir(exist_ok=False)
    source_root = work / 'source'
    shutil.copytree(args.packet / 'current-source', source_root)
    native = source_root / 'GoldSrc-SDK/engine/html/native'
    cpp = native / 'cef_host.cpp'
    source = cpp.read_text()
    original_cpp = source
    source = replace_once(source, 'using namespace cse::browser;', '''using namespace cse::browser;
void CloseTrace(std::uint32_t identifier, const char* event) {
    static unsigned long sequence = 0;
    std::fprintf(stderr, "close_trace=%lu browser=%u event=%s\\n", ++sequence, identifier, event);
    std::fflush(stderr);
}
struct CloseTraceScope {
    std::uint32_t identifier;
    const char* event;
    ~CloseTraceScope() { CloseTrace(identifier, event); }
};
std::vector<std::unique_ptr<CefHostNativeParent>> diagnosticParents;''')
    for signature, label in [
        ('void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {', 'after-created'),
        ('void OnBeforeClose(CefRefPtr<CefBrowser>) override {', 'before-close'),
        ('void Close() {', 'close')]:
        source = replace_once(source, signature, signature + f'''
        CloseTrace(identifier_, "{label}-enter");
        CloseTraceScope trace{{identifier_, "{label}-exit"}};''')
    source = replace_once(source, '                parent.reset();', '''                CloseTrace(0, "parent-task-release");
                parent.reset();''')
    if args.control in ['retain-parent', 'both']:
        source = replace_once(source, '''            CefPostTask(TID_UI, base::BindOnce([](std::unique_ptr<CefHostNativeParent> parent) {
                CloseTrace(0, "parent-task-release");
                parent.reset();
            }, std::move(nativeParent_)));''', '''            CloseTrace(identifier_, "parent-retained-until-shutdown");
            diagnosticParents.push_back(std::move(nativeParent_));''')
    if args.control in ['post-close', 'both']:
        source = replace_once(source, '''        if (closing_) {
            Close();
        } else if (agent_.empty()) {''', '''        if (closing_) {
            CloseTrace(identifier_, "close-posted-from-after-created");
            CefPostTask(TID_UI, base::BindOnce([](CefRefPtr<Client> client) {
                CloseTrace(client->identifier_, "posted-close-running");
                client->Close();
            }, CefRefPtr<Client>(this)));
        } else if (agent_.empty()) {''')
    source = replace_once(source, '    CefShutdown();', '''    CloseTrace(0, "cef-shutdown-enter");
    CefShutdown();
    CloseTrace(0, "cef-shutdown-exit");
    diagnosticParents.clear();
    CloseTrace(0, "retained-parents-cleared");''')
    if args.control == 'candidate':
        source = replace_once(original_cpp, '''        if (closing_) {
            Close();
        } else if (agent_.empty()) {''', '''        if (closing_) {
            // This provider can synchronously destroy the browser from CloseBrowser. Let its
            // creation callback return before closing; the task owns this client until it runs.
            CefPostTask(TID_UI, base::BindOnce([](CefRefPtr<Client> client) {
                client->Close();
            }, CefRefPtr<Client>(this)));
        } else if (agent_.empty()) {''')
    cpp.write_text(source)
    mac = native / 'cef_host_mac.mm'
    source = mac.read_text()
    if args.control != 'candidate':
        source = replace_once(source, '    ~CocoaParent() override {', '''    ~CocoaParent() override {
        std::fprintf(stderr, "close_parent_destroy this=%p window=%p view=%p\\n", this,
            (__bridge void*)window_, (__bridge void*)[window_ contentView]);
        std::fflush(stderr);''')
    mac.write_text(source)
    build = args.root / ('build/cef-host-shared-staging-macos-' + args.configuration.lower())
    app = work / 'cse_cef_host.app'
    staged_app = build / 'host' / args.configuration / 'cse_cef_host.app'
    base_app = args.packet / 'current.app' if args.configuration == 'Debug' else staged_app
    subprocess.run(['cp', '-cR', str(base_app), str(app)], check=True)
    commands = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', 'cse_cef_host'], text=True)
    objects = []
    invocations = []
    for name in ['cef_host.cpp', 'cef_host_mac.mm', 'cef_host_renderer.cpp']:
        command = next(shlex.split(line) for line in commands.splitlines() if ' -c ' in line and
                       line.endswith('/' + name))
        obj = work / (name + '.o')
        command[command.index('-c') + 1] = str(native / name)
        command[command.index('-o') + 1] = str(obj)
        for option in ['-MD', '-MT', '-MF']:
            if option in command:
                index = command.index(option)
                del command[index:index + (1 if option == '-MD' else 2)]
        objects.append(str(obj))
        invocations.append(command)
    executable = app / 'Contents/MacOS/cse_cef_host'
    link_line = next(line for line in commands.splitlines() if line.startswith(': && /usr/bin/c++'))
    link = shlex.split(link_line.split(' && ')[1])
    original_objects = [item for item in link if item.endswith('.o')]
    for original, replacement in zip(original_objects, objects):
        link[link.index(original)] = replacement
    link[link.index('-o') + 1] = str(executable)
    invocations.append(link)
    with (work / 'build.log').open('w') as log:
        for command in invocations:
            log.write(shlex.join(command) + '\n')
            log.flush()
            subprocess.run(command, cwd=build, stdout=log, stderr=subprocess.STDOUT, check=True)
    hashes = {str(path.relative_to(work)): hashlib.sha256(path.read_bytes()).hexdigest()
              for path in [cpp, mac, native / 'cef_host_renderer.cpp', executable,
                           build / 'libcef_dll_wrapper/libcef_dll_wrapper.a'] if path.is_relative_to(work)}
    hashes['external-wrapper'] = hashlib.sha256((build / 'libcef_dll_wrapper/libcef_dll_wrapper.a').read_bytes()).hexdigest()
    (work / 'hashes.json').write_text(json.dumps(hashes, indent=2) + '\n')
    print(executable)


if __name__ == '__main__':
    main()
