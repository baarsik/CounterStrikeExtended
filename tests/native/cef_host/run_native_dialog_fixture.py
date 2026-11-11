"""Controlled native prompt/file interaction; drive the visible OS dialog with the CUA tool.

The fixture never supplies prompt text or file paths through a browser callback. A stage is printed and
written to interaction.json, then the real DOM result is required. Pending-dialog removal cases wait for
an observer-created interaction-observed.json file after the native dialog has actually been inspected.
"""
import argparse
import hashlib
import http.server
import json
from pathlib import Path
import sys
import threading
import time

sys.dont_write_bytecode = True
from run_protocol_fixture import Host, OPERATIONS, delete_fixture_keychain, integer, reap_check, read_string, string

PROMPT_VALUE = 'CSE native prompt accepted'
FILE_NAME = 'cse-native-dialog-selection.txt'
FILE_CONTENT = 'CSE native file contents'
CASES = ('prompt-accept', 'prompt-cancel', 'file-select', 'file-cancel', 'remove-prompt', 'eof-file')
PAGE = b'''<!doctype html><meta charset="utf-8"><title>native-dialog-loading</title>
<style>body{margin:0}button,input{position:absolute;left:8px;width:170px;height:32px}
#prompt{top:8px}#file{top:52px}</style>
<button id="prompt" onclick="document.title='prompt-result:'+String(prompt('CSE controlled native prompt','default'))">
Prompt</button><input type="file" id="file" accept=".txt">
<script>file.onchange=async()=>{document.title='file-result:'+file.files[0].name+':'+await file.files[0].text()};
file.oncancel=()=>{document.title='file-cancelled'};
file.onclick=event=>{document.title='file-click:'+event.isTrusted+':'+navigator.userActivation.isActive};
document.title='native-dialog-ready';</script>'''


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-Type', 'text/html; charset=utf-8')
        self.send_header('Content-Length', str(len(PAGE)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        self.wfile.write(PAGE)

    def log_message(self, *_):
        pass


def run(executable, evidence, selected):
    evidence.mkdir(parents=True, exist_ok=False)
    fixture_id = (hashlib.sha256(executable.read_bytes() + str(evidence).encode()).hexdigest()
                  if sys.platform == 'darwin' else '')
    selection = evidence / FILE_NAME
    selection.write_text(FILE_CONTENT)
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    server.daemon_threads = True
    worker = threading.Thread(target=server.serve_forever, name='cse-native-dialog-http')
    worker.start()
    origin = f'http://127.0.0.1:{server.server_port}'
    policy = lambda url: url == 'about:blank' or url.startswith(origin + '/')
    host = None
    cases = []
    report = None
    cleanup = {}
    try:
        for case in selected:
            storage = evidence / ('storage-' + case)
            host = Host(executable, storage, evidence / (case + '-stderr.log'), fixture_id)
            host.create()
            host.command('PostURL', string(origin + '/fixture') + string(''))
            host.title('native-dialog-ready', policy)
            y = 24 if 'prompt' in case else 68
            target = 'prompt' if 'prompt' in case else 'file'
            host.command('ExecuteJavaScript', string("document.title='hit-target:' + "
                                                    f'document.elementFromPoint(30,{y}).id'))
            host.title('hit-target:' + target, policy)
            host.command('FullRepaint')
            host.wait(lambda event: event[0] == 4, policy)
            host.batch([('SetFocus', integer(1)), ('MouseMove', integer(30) + integer(y)),
                        ('MouseDown', integer(0)), ('MouseUp', integer(0))])
            if target == 'file':
                host.wait(lambda event: event[1] == OPERATIONS['FileLoadDialog'], policy)
            stage = {'case': case, 'host_pid': host.process.pid, 'selection_file': str(selection),
                     'prompt_value': PROMPT_VALUE, 'interaction': 'visible native OS dialog',
                     'observe_then_signal': case in ('remove-prompt', 'eof-file')}
            (evidence / 'interaction.json').write_text(json.dumps(stage, indent=2) + '\n')
            print(json.dumps(stage), flush=True)
            if case in ('remove-prompt', 'eof-file'):
                observed = evidence / 'interaction-observed.json'
                deadline = time.monotonic() + 180
                while not observed.exists() and time.monotonic() < deadline:
                    assert host.process.poll() is None, 'host exited while native dialog should remain open'
                    time.sleep(0.05)
                assert observed.exists(), 'native dialog observation signal was not provided'
                observation = json.loads(observed.read_text())
                assert observation['case'] == case and observation['native_dialog_visible'] is True
                observed.rename(evidence / ('observed-' + case + '.json'))
                if case == 'remove-prompt':
                    host.command('BrowserRemove')
                    host.wait(lambda event: event[1] == OPERATIONS['Close'])
                host.close()
            else:
                expected = {'prompt-accept': 'prompt-result:' + PROMPT_VALUE,
                            'prompt-cancel': 'prompt-result:null',
                            'file-select': 'file-result:' + FILE_NAME + ':' + FILE_CONTENT,
                            'file-cancel': 'file-cancelled'}[case]
                event = host.wait(lambda event: event[1] == OPERATIONS['SetHTMLTitle'] and
                                  read_string(event[4])[0].startswith(('prompt-result:', 'file-result:',
                                                                      'file-cancelled')), policy, timeout=180)
                actual = read_string(event[4])[0]
                (evidence / ('dom-' + case + '.json')).write_text(json.dumps({'expected': expected,
                                                                             'actual': actual}) + '\n')
                assert actual == expected, ('native dialog result mismatch', actual, expected)
                host.command('ExecuteJavaScript', string("document.title='native-dialog-recovered'"))
                host.title('native-dialog-recovered', policy)
                host.close()
                stage['dom_result'] = read_string(event[4])[0]
            stage.update(passed=True, exit_code=host.process.returncode, remaining_helpers=reap_check(storage))
            assert not stage['remaining_helpers']
            cases.append(stage)
            host = None
        report = {'scope': 'real native OS prompt/file interaction and cancellation; no full game acceptance',
                  'executable_sha256': hashlib.sha256(executable.read_bytes()).hexdigest(), 'cases': cases}
    finally:
        if host is not None:
            (evidence / 'failed-events.json').write_text(json.dumps(host.events, indent=2) + '\n')
            if host.process.poll() is None:
                host.process.kill()
            host.process.wait(timeout=10)
            host.worker.join(timeout=10)
            host.stderr.close()
        server.shutdown()
        worker.join(timeout=10)
        server.server_close()
        remaining = [entry for case in CASES for entry in reap_check(evidence / ('storage-' + case))]
        cleanup = {'remaining_helpers': remaining, 'fixture_keychain_id': fixture_id or None}
        if fixture_id and not remaining:
            cleanup.update(delete_fixture_keychain(fixture_id))
        (evidence / 'keychain-cleanup.json').write_text(json.dumps(cleanup, indent=2) + '\n')
        if report is not None:
            report['cleanup'] = cleanup
            (evidence / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
        assert not remaining and (not fixture_id or cleanup.get('deleted')), cleanup
    print('Native prompt/file interaction fixture passed', flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--case', choices=('all',) + CASES, default='all')
    args = parser.parse_args()
    run(args.executable.resolve(), args.evidence.resolve(), CASES if args.case == 'all' else (args.case,))
