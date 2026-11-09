"""Exercise the VGUI order: browser creation followed immediately by ordinary commands."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import threading

from run_protocol_fixture import Host, OPERATIONS, Server, delete_fixture_keychain, integer, reap_check, string


class EarlyServer(Server):
    def handle_error(self, request, client_address):
        # Removing a creating browser can cancel Chromium's speculative sockets.
        if not isinstance(sys.exc_info()[1], ConnectionResetError):
            super().handle_error(request, client_address)


def run(executable, evidence):
    evidence.mkdir(parents=True, exist_ok=False)
    key = hashlib.sha256(executable.read_bytes() + str(evidence).encode()).hexdigest() if sys.platform == 'darwin' else ''
    server = EarlyServer()
    worker = threading.Thread(target=server.serve_forever)
    worker.start()
    host = None
    result = {'success': False, 'executable_sha256': hashlib.sha256(executable.read_bytes()).hexdigest()}
    try:
        host = Host(executable, evidence / 'storage', evidence / 'stderr.log', key)
        url = f'http://127.0.0.1:{server.server_port}/early'
        # One write puts all mutations behind the asynchronous creation, with no readiness wait.
        host.batch([('BrowserCreate', integer(0) + string('CSE Early Browser Fixture')),
                    ('AddHeader', string('X-CSE-Fixture') + string('before-ready')),
                    ('BrowserSize', integer(192) + integer(96)),
                    ('BrowserPosition', integer(7) + integer(11)),
                    ('PostURL', string(url) + string(''))])
        host.title('/early', lambda _: True)
        request = next(row for row in server.requests if row['path'] == '/early')
        assert request['header'] == 'before-ready' and request['agent'] == 'CSE Early Browser Fixture', request
        assert any(row['operation'] == OPERATIONS['BrowserReady'] and row['browser'] == 1 for row in host.events)
        # An owner may remove its widget while CEF is still creating the underlying browser.
        host.batch([('BrowserCreate', integer(0) + string('CSE Removed Before Ready')),
                    ('BrowserSize', integer(24) + integer(16)), ('BrowserRemove', b'')], browser=2)
        host.wait(lambda event: event[1] == OPERATIONS['Close'] and event[2] == 2, lambda _: True)
        host.command('PostURL', string(url + '?still-live') + string(''))
        host.title('/early?still-live', lambda _: True)
        host.close()
        result['success'] = True
    finally:
        server.shutdown()
        server.server_close()
        worker.join(timeout=10)
        if host is not None and host.process.poll() is None:
            try:
                host.process.stdin.close()
            except (BrokenPipeError, OSError):
                pass
            host.process.wait(timeout=30)
        if host is not None:
            host.worker.join(timeout=10)
            assert not host.worker.is_alive()
            assert not reap_check(host.storage)
        if host is not None:
            result.update(provider_exit_code=host.process.returncode, events=host.events, requests=server.requests)
        if key:
            result['keychain_cleanup'] = delete_fixture_keychain(key)
        (evidence / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--evidence', required=True, type=Path)
    args = parser.parse_args()
    run(args.executable.resolve(), args.evidence.resolve())
