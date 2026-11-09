"""Reviewed-only real CFPreferences fixture. Never imported/executed by CTest."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import time
import uuid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', required=True, type=Path)
    parser.add_argument('--evidence', required=True, type=Path)
    parser.add_argument('--run-reviewed-owned-preferences-fixture', action='store_true', required=True)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    args.evidence.mkdir(parents=True, exist_ok=False)
    domain = 'org.counterstrikeextended.fixture.settings.' + uuid.uuid4().hex
    report = {'domain': domain, 'executable': str(executable),
              'sha256': hashlib.sha256(executable.read_bytes()).hexdigest(), 'cases': [], 'success': False,
              'scope': 'real current-user/any-host fixture domain only; process restart, not power-loss durability'}
    active = []

    def start(operation):
        output = (args.evidence / (operation + '.log')).open('wb')
        process = subprocess.Popen([str(executable), '--owned-domain', domain, operation],
                                   stdout=output, stderr=subprocess.STDOUT)
        active.append((process, output))
        return process

    def finish(operation, process):
        code = process.wait(timeout=120)
        report['cases'].append({'operation': operation, 'pid': process.pid, 'exit_code': code})
        if code:
            raise RuntimeError('Owned fixture operation failed: ' + operation)

    try:
        finish('write', start('write'))
        finish('read', start('read'))
        left = start('worker-left')
        right = start('worker-right')
        finish('worker-left', left)
        finish('worker-right', right)
        finish('read-workers', start('read-workers'))
        report['success'] = True
    except Exception as error:
        report['failure'] = str(error)
    finally:
        for process, output in active:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
                report['success'] = False
                report.setdefault('forced_worker_cleanup', []).append(process.pid)
            output.close()
        try:
            finish('cleanup', start('cleanup'))
            finish('verify-empty', start('verify-empty'))
        except Exception as error:
            report['cleanup_failure'] = str(error)
            report['success'] = False
        for process, output in active:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)
                report['success'] = False
            output.close()
        report['children_reaped'] = all(process.poll() is not None for process, _ in active)
        report['completed_unix_seconds'] = time.time()
        (args.evidence / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    return 0 if report['success'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
