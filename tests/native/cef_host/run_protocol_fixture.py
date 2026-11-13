"""Exercise the owned CEF host's real framed protocol, navigation policy and EOF lifecycle."""
import argparse
import hashlib
import http.server
import json
import os
from pathlib import Path
import queue
import re
import struct
import subprocess
import sys
import threading
import time

HEADER = struct.Struct('<4sHHIIII')
U32 = struct.Struct('<I')
ROOT = Path(__file__).resolve().parents[3]
COMMANDS = re.findall(r'\beHTMLCommands_(\w+)\s*[,\n]',
                      (ROOT / 'GoldSrc-SDK/engine/html/htmlmessages.h').read_text())
OPERATIONS = {name: index for index, name in enumerate(COMMANDS)}
FIXTURE_KEYCHAIN_PREFIX = 'CounterStrikeExtended Browser Fixture '
FIXTURE_KEYCHAIN_ACCOUNT = 'CounterStrikeExtended Fixture'


def integer(value):
    return U32.pack(value & 0xffffffff)


def string(value):
    encoded = value.encode('utf-8')
    return integer(len(encoded)) + encoded


def read_string(payload, offset=0):
    length = U32.unpack_from(payload, offset)[0]
    return payload[offset + 4:offset + 4 + length].decode('utf-8'), offset + 4 + length


def fixture_storage_path(storage):
    storage = Path(storage).resolve()
    if sys.platform != 'darwin':
        return storage
    # Keep helper-owned profiles/logs out of protected Documents/Desktop paths.
    # Evidence may live in a source checkout; the runtime uses the app cache.
    cache = Path.home() / 'Library/Caches/CounterStrikeExtended/native-validation/cef-host'
    if storage.is_relative_to(cache):
        return storage
    return cache / hashlib.sha256(str(storage).encode('utf-8')).hexdigest()


def remaining_helpers(storage):
    storage = fixture_storage_path(storage)
    if os.name == 'nt':
        quoted = "'" + str(storage).replace("'", "''") + "'"
        script = ('$needle=' + quoted + '; Get-CimInstance Win32_Process | Where-Object {'
                  '$_.ProcessId -ne $PID -and $_.CommandLine -and '
                  '($_.CommandLine.IndexOf("--cse-storage="+$needle,[StringComparison]::OrdinalIgnoreCase) -ge 0 '
                  '-or $_.CommandLine.IndexOf("--user-data-dir="+$needle,[StringComparison]::OrdinalIgnoreCase) -ge 0)} | '
                  'Select-Object ProcessId,CommandLine | ConvertTo-Json -Compress')
        output = subprocess.check_output(['powershell.exe', '-NoProfile', '-Command', script], text=True).strip()
        rows = json.loads(output) if output else []
        return [rows] if isinstance(rows, dict) else rows
    output = subprocess.check_output(['ps', '-axo', 'pid=,ppid=,args='], text=True)
    needles = ['--cse-storage=' + str(storage), '--user-data-dir=' + str(storage)]
    return [row.strip() for row in output.splitlines()
            if row.strip() and row.split(None, 1)[0] != str(os.getpid()) and
            any(needle in row for needle in needles)]


def reap_check(storage):
    deadline = time.monotonic() + 5
    remaining = remaining_helpers(storage)
    while remaining and time.monotonic() < deadline:
        time.sleep(0.05)
        remaining = remaining_helpers(storage)
    return remaining


def delete_fixture_keychain(identifier):
    assert re.fullmatch('[0-9a-f]{64}', identifier)
    service = FIXTURE_KEYCHAIN_PREFIX + identifier
    result = subprocess.run(['/usr/bin/security', 'delete-generic-password', '-s', service,
                             '-a', FIXTURE_KEYCHAIN_ACCOUNT], capture_output=True, text=True, timeout=10)
    return {'service': service, 'account': FIXTURE_KEYCHAIN_ACCOUNT, 'return_code': result.returncode,
            'stderr': result.stderr.strip(), 'deleted': result.returncode == 0}


class Server(http.server.ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self):
        super().__init__(('127.0.0.1', 0), Handler)
        self.requests = []


class Handler(http.server.BaseHTTPRequestHandler):
    def handle_request(self):
        count = int(self.headers.get('Content-Length', '0'))
        body = self.rfile.read(count)
        self.server.requests.append({'path': self.path, 'method': self.command, 'body': body.decode(),
                                     'header': self.headers.get('X-CSE-Fixture'),
                                     'agent': self.headers.get('User-Agent')})
        if self.path in ('/redirect-allow', '/redirect-deny'):
            self.send_response(302)
            self.send_header('Location', '/allowed' if self.path.endswith('allow') else '/denied')
            self.send_header('Content-Length', '0')
            self.end_headers()
            return
        popup_script = ("<script>fetch('/popup-opener?present='+(window.opener?1:0))</script>"
                        if self.path.startswith('/popup') else '')
        dimensions = 'width:1200px;height:1200px;' if self.path == '/scroll' else ''
        page = ('<!doctype html><meta charset=utf-8><title>' + self.path +
                '</title><body style="' + dimensions + 'background:rgb(17,34,51)">fixture' + popup_script).encode()
        self.send_response(200)
        self.send_header('Content-Type', 'text/html; charset=utf-8')
        self.send_header('Content-Length', str(len(page)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        try:
            self.wfile.write(page)
        except (BrokenPipeError, ConnectionResetError):
            # Closing a tracked browser intentionally cancels in-flight subresources.
            pass

    do_GET = handle_request
    do_POST = handle_request

    def log_message(self, *_):
        pass


class Host:
    def __init__(self, executable, storage, log, fixture_keychain=''):
        self.events = []
        self.navigation = []
        self.last_paint = {}
        self.replies = queue.Queue()
        self.storage = fixture_storage_path(storage)
        self.stderr = log.open('wb')
        command = [str(executable), '--cse-browser-host', f'--cse-storage={self.storage}']
        if fixture_keychain:
            command.append('--cse-fixture-keychain=' + fixture_keychain)
        self.process = subprocess.Popen(command,
                                        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.stderr)
        self.worker = threading.Thread(target=self.read, name='cse-host-framed-output')
        self.worker.start()
        self.wait(lambda event: event[0] == 3 and event[1] == 1)
        if fixture_keychain:
            assert 'cef_host_fixture_keychain=' + fixture_keychain in log.read_text(errors='replace')

    def read(self):
        try:
            while True:
                header = self.exact(HEADER.size)
                if not header:
                    self.replies.put(None)
                    return
                magic, protocol, kind, operation, browser, request, length = HEADER.unpack(header)
                assert magic == b'CSEB' and protocol == 1
                payload = self.exact(length)
                assert len(payload) == length
                self.replies.put((kind, operation, browser, request, payload))
        except BaseException as error:
            self.replies.put(error)

    def exact(self, count):
        result = b''
        while len(result) < count:
            chunk = self.process.stdout.read(count - len(result))
            if not chunk:
                assert not result, 'truncated framed output'
                return result
            result += chunk
        return result

    def send(self, kind, operation, browser=0, payload=b'', request=0):
        self.process.stdin.write(HEADER.pack(b'CSEB', 1, kind, operation, browser, request, len(payload)) + payload)
        self.process.stdin.flush()

    def command(self, name, payload=b'', browser=1):
        self.send(1, OPERATIONS[name], browser, payload)

    def batch(self, commands, browser=1):
        frames = []
        for name, payload in commands:
            frames.append(HEADER.pack(b'CSEB', 1, 1, OPERATIONS[name], browser, 0, len(payload)) + payload)
        self.process.stdin.write(b''.join(frames))
        self.process.stdin.flush()

    def wait(self, predicate, policy=None, timeout=20):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            event = self.replies.get(timeout=max(0.01, deadline - time.monotonic()))
            if isinstance(event, BaseException):
                raise event
            assert event is not None, ('unexpected host EOF', self.process.poll())
            kind, operation, browser, request, payload = event
            row = {'kind': kind, 'operation': operation, 'browser': browser, 'request': request}
            if kind == 2 and operation == OPERATIONS['SetHTMLTitle']:
                row['title'] = read_string(payload)[0]
            if kind == 2 and operation in (OPERATIONS['HorizontalScrollBarSizeResponse'],
                                            OPERATIONS['VerticalScrollBarSizeResponse']):
                row['geometry'] = list(struct.unpack_from('<6i', payload))
            self.events.append(row)
            if kind == 2 and operation == OPERATIONS['StartRequest']:
                url, offset = read_string(payload)
                target, offset = read_string(payload, offset)
                post, offset = read_string(payload, offset)
                self.navigation.append({'url': url, 'target': target, 'post': post,
                                        'redirect': U32.unpack_from(payload, offset)[0] != 0})
            if kind == 4:
                self.last_paint[browser] = payload[:4]
                self.command('NeedsPaintResponse', payload[:4], browser)
            if predicate(event):
                return event
            if kind == 2 and operation == OPERATIONS['StartRequest'] and policy is not None:
                url, _ = read_string(payload)
                self.command('StartRequestResponse', integer(policy(url)), browser)
        raise AssertionError('event deadline exceeded')

    def title(self, value, policy=None):
        return self.wait(lambda event: event[1] == OPERATIONS['SetHTMLTitle'] and
                         read_string(event[4])[0] == value, policy)

    def create(self, browser=1):
        self.command('BrowserCreate', integer(0) + string('CSE Protocol Fixture'), browser)
        self.wait(lambda event: event[1] == OPERATIONS['BrowserReady'] and event[2] == browser)
        self.command('BrowserSize', integer(192) + integer(96), browser)

    def close(self, expected=0):
        self.process.stdin.close()
        # Drain frames while CEF closes browsers, flushes cookies, reaps helpers and closes stdout.
        while True:
            event = self.replies.get(timeout=30)
            if isinstance(event, BaseException):
                raise event
            if event is None:
                break
        assert self.process.wait(timeout=10) == expected
        self.worker.join(timeout=10)
        assert not self.worker.is_alive()
        self.process.stdout.close()
        self.stderr.close()
        assert not reap_check(self.storage), 'CEF helper survived host shutdown'


def run(executable, evidence, production_keychain=False):
    evidence.mkdir(parents=True, exist_ok=False)
    fixture_keychain = (hashlib.sha256(executable.read_bytes() + str(evidence).encode('utf-8')).hexdigest()
                        if sys.platform == 'darwin' and not production_keychain else '')
    storage_paths = [evidence / name for name in ('storage', 'invalid-storage', 'truncated-storage')]
    server = Server()
    worker = threading.Thread(target=server.serve_forever, name='cse-host-http')
    worker.start()
    origin = f'http://127.0.0.1:{server.server_port}'
    cases = []
    host = None
    report = None
    try:
        for invalid in ('a' * 63, 'A' * 64):
            rejected = subprocess.run([str(executable), '--cse-browser-host',
                f'--cse-storage={evidence / "invalid-keychain-id"}', '--cse-fixture-keychain=' + invalid],
                capture_output=True, timeout=20)
            assert rejected.returncode == 2 and rejected.stdout == b'', 'invalid fixture namespace was accepted'
        host = Host(executable, evidence / 'storage', evidence / 'protocol-stderr.log', fixture_keychain)
        host.create()
        host.command('AddHeader', string('X-CSE-Fixture') + string('preserved'))
        host.command('PostURL', string(origin + '/pending') + string(''))
        pending = host.wait(lambda event: event[1] == OPERATIONS['StartRequest'])
        assert read_string(pending[4])[0] == origin + '/pending'
        host.send(3, 2, payload=string(origin) + string('owned') + string('persisted') + string('/') +
                  integer(int(time.time()) + 3600))
        host.send(3, 3, payload=string(origin + '/') + string('127.0.0.1'), request=1)
        cookies = host.wait(lambda event: event[0] == 3 and event[1] == 4 and event[3] == 1)
        assert 'owned=persisted' in read_string(cookies[4])[0]
        assert not any(row['path'] == '/pending' for row in server.requests)
        host.command('StartRequestResponse', integer(0))
        host.command('PostURL', string(origin + '/post') + string('alpha=one&beta=two'))
        host.title('/post', lambda _: True)
        post = next(row for row in server.requests if row['path'] == '/post')
        assert post['method'] == 'POST' and post['body'] == 'alpha=one&beta=two', post
        assert post['header'] == 'preserved' and post['agent'] == 'CSE Protocol Fixture', post
        assert host.navigation[-1]['post'] == 'alpha=one&beta=two'
        for requested, path, body in [
                (origin, '/', 'normalized=root'),
                (origin.replace('http://', 'HTTP://') + '/spelling/../canonical?value=one two',
                 '/canonical?value=one%20two', 'normalized=path')]:
            before = len(server.requests)
            host.command('PostURL', string(requested) + string(body))
            host.title(path, lambda _: True)
            request = next(row for row in server.requests[before:] if row['path'] == path)
            assert request['method'] == 'POST' and request['body'] == body, request
            assert request['header'] == 'preserved' and request['agent'] == 'CSE Protocol Fixture', request
            assert host.navigation[-1]['post'] == body, host.navigation[-1]
        # A working DOM alone does not prove the compositor has an automatic frame source.
        # Require its first animation and pixel frame before the later queue-ordering burst.
        host.command('ExecuteJavaScript', string("requestAnimationFrame(()=>document.title='initial-frame-ready')"))
        host.title('initial-frame-ready')
        host.command('FullRepaint')
        host.wait(lambda event: event[0] == 4)
        host.command('PostURL', string(origin + '/stopped-policy') + string(''))
        host.wait(lambda event: event[1] == OPERATIONS['StartRequest'] and
                  read_string(event[4])[0] == origin + '/stopped-policy')
        # Backlog normal UI commands, then cancel and send the stale allow in one input batch.
        # An inline reader response would overtake these mutations and authorize the stopped request.
        commands = [('BrowserPosition', integer(0) + integer(0)) for _ in range(512)]
        commands += [('StopLoad', b''), ('PostURL', string(origin + '/replacement-policy') + string('')),
                     ('StartRequestResponse', integer(1))]
        host.batch(commands)
        host.wait(lambda event: event[1] == OPERATIONS['StartRequest'] and
                  read_string(event[4])[0] == origin + '/replacement-policy')
        host.command('StartRequestResponse', integer(0))
        host.command('PostURL', string(origin + '/keys') + string(''))
        host.title('/keys', lambda _: True)
        assert not any(row['path'] in ('/stopped-policy', '/replacement-policy') for row in server.requests)
        host.command('ExecuteJavaScript', string("document.body.tabIndex=0;document.body.focus();"
            "document.addEventListener('keydown',event=>{if(event.key==='Enter')"
            "document.title='key-location:'+event.location+':'+event.code});document.title='keys-ready'"))
        host.title('keys-ready')
        host.command('FullRepaint')
        host.wait(lambda event: event[0] == 4)
        host.command('SetFocus', integer(1))
        host.command('MouseMove', integer(20) + integer(10))
        host.command('MouseDown', integer(0))
        host.command('MouseUp', integer(0))
        host.command('KeyDown', integer(0x0d) + integer(8))
        host.command('KeyUp', integer(0x0d) + integer(8))
        host.title('key-location:3:NumpadEnter')
        host.command('KeyDown', integer(0x0d) + integer(0))
        host.command('KeyUp', integer(0x0d) + integer(0))
        host.title('key-location:0:Enter')
        if sys.platform == 'darwin':
            host.command('ExecuteJavaScript', string("document.addEventListener('keydown',event=>{"
                "if(event.code==='KeyA'){event.preventDefault();document.title='focus-key:'+"
                "event.metaKey+':'+event.ctrlKey+':'+event.altKey+':'+event.shiftKey}});"
                "document.title='focus-keys-ready'"))
            host.title('focus-keys-ready')
            for command_key in (0x5b, 0x5c):
                host.batch([('KeyDown', integer(command_key) + integer(0)),
                            ('KeyDown', integer(0x41) + integer(0)),
                            ('KeyUp', integer(0x41) + integer(0))])
                host.title('focus-key:true:false:false:false')
                # The Command release goes to another VGUI control, so no KeyUp reaches HTML.
                host.batch([('SetFocus', integer(0)), ('SetFocus', integer(1)),
                            ('KeyDown', integer(0x41) + integer(0)),
                            ('KeyUp', integer(0x41) + integer(0))])
                host.title('focus-key:false:false:false:false')
        host.command('PostURL', string(origin + '/scroll') + string(''))
        host.title('/scroll', lambda _: True)
        host.command('SetHorizontalScroll', integer(70))
        host.command('SetVerticalScroll', integer(100))
        host.command('ExecuteJavaScript', string("requestAnimationFrame(()=>document.title='scroll-moved')"))
        host.title('scroll-moved')
        host.command('FullRepaint')
        host.wait(lambda event: event[1] == OPERATIONS['HorizontalScrollBarSizeResponse'] and
                  U32.unpack_from(event[4], 16)[0] == 70 and U32.unpack_from(event[4], 20)[0] > 0)
        host.wait(lambda event: event[1] == OPERATIONS['VerticalScrollBarSizeResponse'] and
                  U32.unpack_from(event[4], 16)[0] == 100 and U32.unpack_from(event[4], 20)[0] > 0)
        invalid_geometry = ('[-2147483648,96,2147483647,96,0,0]',
                            '[192,96,1000,1000,1e300,0]', '[192.5,96,1000,1000,0,0]',
                            '[192,96,"wrong",1000,0,0]', '[192,96,2147483647,1000,0,0]')
        for index, invalid in enumerate(invalid_geometry):
            before = len(host.events)
            host.command('ExecuteJavaScript', string("window.savedStringify=JSON.stringify;"
                "JSON.stringify=function(){document.title='geometry-evaluated-" + str(index) +
                "';return " + json.dumps(invalid) + "};document.title='geometry-installed-" + str(index) + "'"))
            host.title('geometry-installed-' + str(index))
            host.command('FullRepaint')
            host.title('geometry-evaluated-' + str(index))
            host.command('ExecuteJavaScript', string("JSON.stringify=window.savedStringify;"
                "document.title='geometry-restored-" + str(index) + "'"))
            host.title('geometry-restored-' + str(index))
            host.command('FullRepaint')
            host.wait(lambda event: event[1] == OPERATIONS['HorizontalScrollBarSizeResponse'] and
                      U32.unpack_from(event[4], 8)[0] == 192)
            # This controlled page has 1200px content. Poisoned metadata must never reach the UI stream.
            for event in host.events[before:]:
                if 'geometry' in event:
                    assert all(0 <= value <= 8192 for value in event['geometry']), event
        host.command('ExecuteJavaScript', string("alert('cancelled-alert')"))
        host.wait(lambda event: event[1] == OPERATIONS['JSAlert'])
        host.command('PostURL', string(origin + '/dialog-replacement') + string(''))
        host.title('/dialog-replacement', lambda _: True)
        host.command('ExecuteJavaScript', string("document.title=confirm('replacement-confirm')?"
                                                "'failure:stale-dialog-approval':'replacement-dialog-denied'"))
        host.wait(lambda event: event[1] == OPERATIONS['JSConfirm'])
        # Reply to the cancelled alert, then deny the current confirm. The old reply cannot approve the new dialog.
        host.batch([('JSDialogResponse', integer(1)), ('JSDialogResponse', integer(0))])
        host.title('replacement-dialog-denied')

        host.command('PostURL', string(origin + '/redirect-allow') + string(''))
        host.title('/allowed', lambda _: True)
        host.command('PostURL', string(origin + '/redirect-deny') + string(''))
        denied_redirect = host.wait(lambda event: event[1] == OPERATIONS['StartRequest'] and
                                   read_string(event[4])[0] == origin + '/denied', lambda _: True)
        # Redirect flag follows the URL, target-frame and POST strings.
        _, offset = read_string(denied_redirect[4])
        _, offset = read_string(denied_redirect[4], offset)
        _, offset = read_string(denied_redirect[4], offset)
        assert U32.unpack_from(denied_redirect[4], offset)[0] == 1
        host.send(3, 3, payload=string(origin + '/') + string(''), request=3)
        cookies = host.wait(lambda event: event[0] == 3 and event[1] == 4 and event[3] == 3)
        assert 'owned=persisted' in read_string(cookies[4])[0]
        host.command('StartRequestResponse', integer(0))
        # A marker command proves the rejected redirect has completed without touching the denied URL.
        host.command('ExecuteJavaScript', string("document.title='redirect-rejected'"))
        host.title('redirect-rejected')
        assert not any(row['path'] == '/denied' for row in server.requests)
        local_page = evidence / 'motd.html'
        local_page.write_text('<!doctype html><meta charset=utf-8><title>local-motd</title>')
        host.command('PostURL', string(local_page.as_uri()) + string(''))
        host.title('local-motd', lambda _: True)
        host.command('PostURL', string('data:text/html,<title>data-motd</title>') + string(''))
        host.title('data-motd', lambda _: True)
        host.command('PostURL', string(origin + '/popup-parent') + string(''))
        host.title('/popup-parent', lambda _: True)
        host.command('ExecuteJavaScript', string("window.popupButton=document.createElement('button');"
            "popupButton.style='position:absolute;left:8px;top:8px;width:120px;height:40px';"
            "popupButton.textContent='Open popup';document.body.appendChild(popupButton);"
            "popupButton.onclick=()=>{window.fixturePopup=window.open('" + origin +
            "/popup','cse-fixture','width=320,height=200')};document.title='popup-button-ready'"))
        host.title('popup-button-ready')
        host.command('FullRepaint')
        host.wait(lambda event: event[0] == 4)
        host.command('SetFocus', integer(1))
        host.command('MouseMove', integer(30) + integer(20))
        host.command('MouseDown', integer(0))
        host.command('MouseUp', integer(0))
        popup = host.wait(lambda event: event[1] == OPERATIONS['PopupHTMLWindow'])
        assert read_string(popup[4])[0] == origin + '/popup'
        host.wait(lambda event: event[1] == OPERATIONS['StartRequest'] and
                  read_string(event[4])[0] == origin + '/popup')
        host.command('StartRequestResponse', integer(1))
        deadline = time.monotonic() + 10
        while not any(row['path'] == '/popup-opener?present=1' for row in server.requests):
            assert time.monotonic() < deadline, 'native popup lost its real opener relationship'
            time.sleep(0.01)
        assert host.navigation[-1]['target'] == '_blank'
        host.command('ExecuteJavaScript', string("document.title=fixturePopup&&!fixturePopup.closed?"
                                                "'native-popup-open':'failure:popup-closed'"))
        host.title('native-popup-open')
        host.command('ExecuteJavaScript', string("fixturePopup.close();document.title='native-popup-close-requested'"))
        host.title('native-popup-close-requested')
        host.command('ExecuteJavaScript', string("popupButton.onclick=()=>{window.remainingPopup=window.open('" +
            origin + "/popup-remaining','cse-remaining','width=320,height=200')};document.title='popup-button-again'"))
        host.title('popup-button-again')
        host.command('FullRepaint')
        host.wait(lambda event: event[0] == 4)
        host.command('MouseDown', integer(0))
        host.command('MouseUp', integer(0))
        host.wait(lambda event: event[1] == OPERATIONS['PopupHTMLWindow'])
        host.wait(lambda event: event[1] == OPERATIONS['StartRequest'] and
                  read_string(event[4])[0] == origin + '/popup-remaining')
        host.command('BrowserRemove')
        host.wait(lambda event: event[1] == OPERATIONS['Close'] and event[2] == 1)
        host.command('NeedsPaintResponse', host.last_paint[1])
        host.command('JSDialogResponse', integer(0))
        host.create(2)
        host.command('PostURL', string(origin + '/eof-pending') + string(''), 2)
        host.wait(lambda event: event[1] == OPERATIONS['StartRequest'] and event[2] == 2)
        host.close()
        cases.append({'name': 'pending-cookie-post-redirect-eof', 'passed': True, 'events': host.events, 'navigation': host.navigation})
        popup_log = (evidence / 'protocol-stderr.log').read_text()
        assert popup_log.count('cef_host_native_popup_created=') == 2, popup_log
        assert popup_log.count('cef_host_native_popup_closed=') == 2, popup_log
        assert popup_log.count('cef_host_ignored_dom_reply=geometry_bounds') >= len(invalid_geometry), popup_log
        host = Host(executable, evidence / 'storage', evidence / 'restart-stderr.log', fixture_keychain)
        host.send(3, 3, payload=string(origin + '/') + string(''), request=2)
        cookies = host.wait(lambda event: event[0] == 3 and event[1] == 4 and event[3] == 2)
        assert 'owned=persisted' in read_string(cookies[4])[0]
        host.close()
        cases.append({'name': 'cookie-persisted-second-process', 'passed': True})
        host = Host(executable, evidence / 'invalid-storage', evidence / 'unknown-mutation-stderr.log', fixture_keychain)
        host.command('PostURL', string(origin + '/unknown-browser') + string(''), browser=99)
        host.close(expected=1)
        assert not any(row['path'] == '/unknown-browser' for row in server.requests)
        cases.append({'name': 'unknown-browser-mutation-rejected', 'passed': True})
        host = Host(executable, evidence / 'truncated-storage', evidence / 'truncated-frame-stderr.log', fixture_keychain)
        host.process.stdin.write(b'CSE')
        host.process.stdin.flush()
        host.close(expected=1)
        cases.append({'name': 'truncated-input-frame-rejected', 'passed': True})
        report = {'cases': cases, 'requests': server.requests,
                  'storage_paths': [str(fixture_storage_path(path)) for path in storage_paths],
                  'executable_sha256': hashlib.sha256(executable.read_bytes()).hexdigest(),
                  'fixture_keychain_id': fixture_keychain or None}

    finally:
        if host:
            (evidence / 'observed.json').write_text(json.dumps({'events': host.events,
                'navigation': host.navigation, 'requests': server.requests}, indent=2) + '\n')
        if host and host.process.poll() is None:
            # Test cleanup only: the host itself must exit cooperatively on stdin EOF.
            try:
                host.process.stdin.close()
            except (BrokenPipeError, ValueError):
                pass
            try:
                host.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                host.process.kill()
                host.process.wait()
        server.shutdown()
        worker.join()
        server.server_close()
        remaining = [row for storage in storage_paths for row in reap_check(storage)]
        cleanup = {'fixture_keychain_id': fixture_keychain or None, 'remaining_helpers': remaining}
        if fixture_keychain:
            if remaining:
                cleanup['skipped'] = 'owned helpers are still alive'
            else:
                try:
                    cleanup.update(delete_fixture_keychain(fixture_keychain))
                except (OSError, subprocess.TimeoutExpired) as error:
                    cleanup['error'] = str(error)
            (evidence / 'keychain-cleanup.json').write_text(json.dumps(cleanup, indent=2) + '\n')
        if report is not None:
            report['cleanup'] = cleanup
            (evidence / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
        if remaining or (fixture_keychain and report is not None and not cleanup.get('deleted')):
            raise AssertionError(('fixture cleanup incomplete', cleanup))
    print('Owned CEF wire fixture passed pending-policy cookie RPC, POST/headers/agent, redirect allow/deny, '
          'native popup/opener/parent cleanup, multiple browsers, EOF cancellation and persistent cookie restart')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--evidence', required=True, type=Path)
    parser.add_argument('--production-keychain', action='store_true',
                        help='Use production storage identity only for an explicitly sealed final build')
    arguments = parser.parse_args()
    run(arguments.executable.resolve(strict=True), arguments.evidence.resolve(), arguments.production_keychain)
