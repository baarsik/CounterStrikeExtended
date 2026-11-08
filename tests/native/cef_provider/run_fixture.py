"""Run two sandboxed CEF processes against a fixture-owned loopback HTTP server."""
import argparse
import hashlib
import http.server
import json
import os
from pathlib import Path
import subprocess
import threading
import time


PAGE = b'''<!doctype html><meta charset="utf-8"><title>fixture-loading</title>
<style>html,body{margin:0;width:100%;height:100%;background:rgb(17,34,51)}
input{position:absolute;left:8px;top:8px;width:80px;height:32px;box-sizing:border-box}
button{position:absolute;left:96px;top:8px;width:64px;height:32px}</style>
<input id="input" oninput="document.title='input:'+this.value">
<button onclick="document.title='mouse-click'">Click</button>
<script>
async function bootstrap(phase) {
  try {
    if (phase === 'read' && (localStorage.getItem('cse') !== 'persisted' ||
        !document.cookie.split('; ').includes('cse=persisted'))) {
      throw new Error('persistent cookie or local storage missing');
    }
    if (phase === 'write') {
      localStorage.setItem('cse', 'persisted');
      document.cookie='cse=persisted; Path=/; Max-Age=3600; SameSite=Strict';
    }
    const cached = await (await fetch('/cached')).text();
    if (cached !== 'cse-persistent-http-cache') throw new Error('cache payload');
    document.title='fixture-ready';
  } catch(error) { document.title='failure:'+error.message; }
}
function focusInput() {
  document.getElementById('input').focus();
  document.title=document.activeElement.id==='input'?'input-focused':'failure:focus';
}
</script>'''


class FixtureServer(http.server.ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self):
        super().__init__(('127.0.0.1', 0), Handler)
        self.requests = []


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        self.server.requests.append({'path': self.path, 'cookie_present':
                                     'cse=persisted' in self.headers.get('Cookie', '')})
        if self.path == '/fixture':
            body = PAGE
            cache = 'no-store'
            content_type = 'text/html; charset=utf-8'
        elif self.path == '/cached':
            body = b'cse-persistent-http-cache'
            cache = 'public, max-age=31536000'
            content_type = 'text/plain'
        else:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', cache)
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_):
        pass


def remaining_helpers(storage):
    if os.name == 'nt':
        command = ['powershell.exe', '-NoProfile', '-Command',
                   "Get-CimInstance Win32_Process -Filter \"Name = 'cse_cef_probe.exe'\" | "
                   'Select-Object ProcessId,CommandLine | ConvertTo-Json -Compress']
        output = subprocess.check_output(command, text=True).strip()
        processes = json.loads(output) if output else []
        if isinstance(processes, dict):
            processes = [processes]
        return [row for row in processes if str(storage).lower() in (row['CommandLine'] or '').lower()]
    output = subprocess.check_output(['ps', '-axo', 'pid=,ppid=,args='], text=True)
    return [line.strip() for line in output.splitlines() if str(storage) in line and 'cse_cef_probe' in line]


def run(executable, evidence):
    evidence.mkdir(parents=True, exist_ok=False)
    storage = evidence / 'storage'
    storage.mkdir()
    server = FixtureServer()
    worker = threading.Thread(target=server.serve_forever, name='cse-cef-http')
    worker.start()
    cases = []
    try:
        for phase in ('write', 'read'):
            command = [str(executable), f'--cse-url=http://127.0.0.1:{server.server_port}/fixture',
                       f'--cse-phase={phase}', f'--cse-storage={storage}']
            before = len(server.requests)
            start = time.monotonic()
            with (evidence / f'{phase}-stdout.log').open('wb') as stdout, \
                    (evidence / f'{phase}-stderr.log').open('wb') as stderr:
                completed = subprocess.run(command, stdout=stdout, stderr=stderr, timeout=60, check=False)
            case = {'phase': phase, 'exit_code': completed.returncode,
                    'elapsed_seconds': time.monotonic() - start, 'requests': server.requests[before:]}
            cases.append(case)
            (evidence / 'cases.json').write_text(json.dumps(cases, indent=2) + '\n')
            assert completed.returncode == 0, case
            output = (evidence / f'{phase}-stdout.log').read_text(encoding='utf-8')
            assert 'native_cef_provider_probe_passed=1' in output, output
            deadline = time.monotonic() + 5
            while remaining_helpers(storage) and time.monotonic() < deadline:
                time.sleep(0.05)
            assert not remaining_helpers(storage), 'CEF helper survived successful shutdown'
            case['remaining_helpers'] = 0
        cached_requests = [row for row in server.requests if row['path'] == '/cached']
        assert len(cached_requests) == 1, ('persistent HTTP cache missed', cached_requests)
        assert any(row['path'] == '/fixture' and row['cookie_present'] for row in cases[1]['requests'])
        assert (storage / 'profile').is_dir()
        report = {'cases': cases, 'persistent_cache_server_hits': len(cached_requests),
                  'executable_sha256': hashlib.sha256(executable.read_bytes()).hexdigest(),
                  'scope': 'SDK OSR/input/JavaScript/persistent storage/lifecycle; no game HTML integration'}
        (evidence / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
        print('Native CEF fixture passed two process lifetimes, pixels, input, cookies and persistent HTTP cache')
    finally:
        server.shutdown()
        worker.join()
        server.server_close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--evidence', required=True, type=Path)
    args = parser.parse_args()
    run(args.executable.resolve(strict=True), args.evidence.resolve())
