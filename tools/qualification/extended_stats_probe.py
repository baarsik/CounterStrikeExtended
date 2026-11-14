"""Prepare and inspect an owned AMXX server probe; no inferred client/relay acceptance."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import time

VALUES = [0, 1, 255, 256, 511, 512, 999, 1000, 32767, 32768, 65535, 65536, 99999, 100000]


def analyze(text):
    starts = list(re.finditer(r'CSE_PROBE init health_id=(\d+) armor_id=(\d+) invalid_identity=(\d+)', text))
    # Analyze only the latest map/plugin instance; never borrow success from an earlier run.
    current = text[starts[-1].start():] if starts else text
    init = starts[-1] if starts else None
    cases = [tuple(map(int, match)) for match in re.findall(
        r'CSE_PROBE case subject=(\d+) expected=(\d+) health=(-?\d+) armor=(-?\d+) alive=(\d+)', current)]
    wires = [dict(name=m[0], destination=int(m[1]), recipient=int(m[2]), subject=int(m[3]),
                  amount=int(m[4]), time=float(m[5])) for m in re.findall(
        r'CSE_PROBE wire name=(HealthEx|BatteryEx) dest=(\d+) recipient=(\d+) subject=(\d+) amount=(-?\d+) time=([0-9.]+)', current)]
    complete = re.findall(r'CSE_PROBE complete subject=(\d+)', current)
    subject = int(complete[-1]) if complete else None
    exact_cases = [case for case in cases if case[0] == subject]
    case_pass = ([case[1] for case in exact_cases] == VALUES and
                 all(expected == health == armor and alive == (expected > 0)
                     for _, expected, health, armor, alive in exact_cases))
    observed = {name: sorted({wire['amount'] for wire in wires
                             if wire['name'] == name and wire['subject'] == subject and wire['destination'] == 9})
                for name in ('HealthEx', 'BatteryEx')}
    payloads_pass = all(all(value in amounts for value in VALUES) for amounts in observed.values())
    registered = bool(init and int(init[1]) > 0 and int(init[2]) > 0 and init[1] != init[2] and init[3] == '0')
    errors = re.findall(r'CSE_PROBE (?:error|malformed)[^\r\n]*', current)
    return {'module_and_native_resolution': registered, 'boundary_gameplay_readback': case_pass,
            'server_msg_spec_payloads': payloads_pass, 'subject': subject, 'cases': exact_cases,
            'observed_values': observed, 'wire_callbacks': wires, 'errors': errors,
            'server_probe_pass': registered and case_pass and payloads_pass and not errors,
            'client_visual_acceptance': 'unverified', 'stock_client_acceptance': 'unverified',
            'ordinary_spectator_acceptance': 'unverified', 'hltv_late_join_and_demo_acceptance': 'unverified'}


def rcon(address, password, command):
    host, port = address.rsplit(':', 1)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as connection:
        connection.settimeout(3)
        connection.connect((host, int(port)))
        connection.send(b'\xff' * 4 + b'challenge rcon\n')
        challenge = connection.recv(65536)
        match = re.search(rb'challenge rcon (-?\d+)', challenge)
        if not match:
            raise RuntimeError('server did not return an RCON challenge')
        if any(char in password for char in '\"\r\n'):
            raise ValueError('unsupported password characters')
        connection.send(b'\xff' * 4 + b'rcon ' + match[1] + b' "' + password.encode() + b'" ' + command.encode() + b'\n')
        packets = []
        while True:
            try:
                packets.append(connection.recv(65536))
            except socket.timeout:
                break
        return b'\n'.join(packets).decode('utf-8', errors='replace')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare', type=Path, help='AMXX scripting directory; write probe source/include here')
    parser.add_argument('--compiler', type=Path, help='AMXX compiler, optionally compile the prepared source')
    parser.add_argument('--server', help='owned test server host:port; run probe via RCON')
    parser.add_argument('--password-env', default='CSE_PROBE_RCON_PASSWORD', help='RCON secret environment variable')
    parser.add_argument('--subject', type=int, default=0, help='0 creates a test bot; otherwise an owned test player')
    parser.add_argument('--client-command', help='optional JSON argument array to launch a test client')
    parser.add_argument('--relay-command', help='optional JSON argument array to launch an existing HLTV relay')
    parser.add_argument('--wait', type=float, default=12, help='seconds to collect after RCON trigger (max 60)')
    parser.add_argument('--log', type=Path, help='local captured console log to analyze after the run')
    parser.add_argument('--module', type=Path, action='append', default=[], help='record inspected module hash/magic')
    parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args()
    args.evidence.mkdir(parents=True, exist_ok=True)
    root = Path(__file__).resolve().parents[2]
    if args.prepare:
        args.prepare.mkdir(parents=True, exist_ok=True)
        (args.prepare / 'include').mkdir(exist_ok=True)
        shutil.copyfile(Path(__file__).with_suffix('.sma'), args.prepare / 'extended_stats_probe.sma')
        shutil.copyfile(root / 'src/amxx/cse_extstats/cse_extstats.inc', args.prepare / 'include/cse_extstats.inc')
        if args.compiler:
            result = subprocess.run([str(args.compiler.resolve()), 'extended_stats_probe.sma',
                                     '-oextended_stats_probe.amxx'], cwd=args.prepare, capture_output=True, text=True)
            (args.evidence / 'compiler.log').write_text(result.stdout + result.stderr)
            result.check_returncode()
    children = []
    try:
        for name in ('client_command', 'relay_command'):
            value = getattr(args, name)
            if value:
                command = json.loads(value)
                if not isinstance(command, list) or not command or not all(isinstance(arg, str) for arg in command):
                    raise ValueError('launch command must be a JSON array of arguments')
                children.append(subprocess.Popen(command))
        if args.server:
            password = os.environ[args.password_env]
            console = []
            for command in ('version', 'amxx version', 'amxx modules', 'amxx plugins', f'cse_probe_run {args.subject}'):
                console.append(rcon(args.server, password, command))
            (args.evidence / 'rcon.log').write_text('\n'.join(console))
            time.sleep(max(0, min(args.wait, 60)))
        text = args.log.read_text(errors='replace') if args.log else ''
        result = analyze(text)
        result['modules'] = [{'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                              'magic': path.read_bytes()[:5].hex()} for path in args.module]
        if args.log:
            shutil.copyfile(args.log, args.evidence / 'console.log')
        (args.evidence / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps({key: value for key, value in result.items() if key not in ('wire_callbacks', 'cases')}, indent=2))
        return 0 if result['server_probe_pass'] or (args.prepare and not args.log and not args.server) else 2
    finally:
        for child in children:
            if child.poll() is None:
                child.terminate()
                try:
                    child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait()


if __name__ == '__main__':
    raise SystemExit(main())
