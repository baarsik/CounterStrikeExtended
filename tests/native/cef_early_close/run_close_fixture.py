"""Measure owned sandboxed CEF early-close lifetime without engine/production edits."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import threading
import time
import traceback

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'cef_host'))
from run_protocol_fixture import Host, OPERATIONS, Server, delete_fixture_keychain, integer, reap_check, string


def run(executable, evidence, mode):
    evidence.mkdir(parents=True, exist_ok=False)
    key = hashlib.sha256(executable.read_bytes() + str(evidence).encode()).hexdigest()
    server = Server()
    worker = threading.Thread(target=server.serve_forever)
    worker.start()
    host = None
    started = time.time()
    result = {'success': False, 'mode': mode, 'executable_sha256': hashlib.sha256(executable.read_bytes()).hexdigest()}
    try:
        host = Host(executable, evidence / 'storage', evidence / 'stderr.log', key)
        result['host_pid'] = host.process.pid
        host.create()
        url = f'http://127.0.0.1:{server.server_port}'
        host.command('PostURL', string(url + '/before') + string(''))
        host.title('/before', lambda _: True)
        if mode == 'ready':
            host.create(browser=2)
            host.command('BrowserRemove', browser=2)
        else:
            commands = [('BrowserCreate', integer(0) + string('CSE Early Close Fixture'))]
            if mode == 'sized':
                commands.append(('BrowserSize', integer(24) + integer(16)))
            commands.append(('BrowserRemove', b''))
            host.batch(commands, browser=2)
        host.wait(lambda event: event[1] == OPERATIONS['Close'] and event[2] == 2, lambda _: True)
        result['close_received'] = True
        result['removed_browser_ready'] = any(row['operation'] == OPERATIONS['BrowserReady'] and
                                               row['browser'] == 2 for row in host.events)
        host.command('PostURL', string(url + '/after') + string(''))
        host.title('/after', lambda _: True)
        result['anchor_navigation_after_close'] = True
        host.close()
        result['success'] = True
    except BaseException as error:
        result['exception'] = repr(error)
        result['traceback'] = traceback.format_exc()
    finally:
        server.shutdown()
        server.server_close()
        worker.join(timeout=10)
        if host is not None:
            if host.process.poll() is None:
                try:
                    host.process.stdin.close()
                except (BrokenPipeError, OSError):
                    pass
                try:
                    host.process.wait(timeout=30)
                except BaseException as error:
                    result['shutdown_error'] = repr(error)
                    host.process.kill()
                    host.process.wait(timeout=10)
            result['exit_code'] = host.process.returncode
            result['events'] = host.events
            host.worker.join(timeout=10)
            result['reader_stopped'] = not host.worker.is_alive()
            result['remaining_helpers'] = reap_check(host.storage)
            host.stderr.close()
        result['requests'] = server.requests
        result['keychain_cleanup'] = delete_fixture_keychain(key)
        if result['success']:
            result['success'] = (result.get('exit_code') == 0 and result.get('reader_stopped') and
                                 not result.get('remaining_helpers') and
                                 result['keychain_cleanup']['return_code'] in (0, 44))
        reports = Path.home() / 'Library/Logs/DiagnosticReports'
        if reports.exists() and host is not None:
            for report in reports.glob('cse_cef_host*.ips'):
                if report.stat().st_mtime < started:
                    continue
                raw = report.read_text()
                try:
                    body = json.loads(raw[raw.index('\n') + 1:])
                except (ValueError, json.JSONDecodeError):
                    continue
                if body.get('pid') == host.process.pid:
                    (evidence / 'owned-crash.ips').write_text(raw)
                    result['crash_report'] = report.name
        (evidence / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({name: result.get(name) for name in ['success', 'mode', 'close_received',
                      'removed_browser_ready', 'anchor_navigation_after_close', 'exit_code', 'exception']}))
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--mode', choices=['minimal', 'sized', 'ready'], default='minimal')
    args = parser.parse_args()
    report = run(args.executable.resolve(), args.evidence.resolve(), args.mode)
    raise SystemExit(0 if report['success'] else 1)
