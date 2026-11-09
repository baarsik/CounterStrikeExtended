"""Owned reconnect/changelevel fixture; frozen decoders and transport are pinned inputs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import time


def write_cfg(game, name, commands):
    if not re.fullmatch(r'[A-Za-z0-9_-]+\.cfg', name):
        raise ValueError('cfg name must be a local basename')
    if any(not command or any(not ' ' <= character <= '~' for character in command) for command in commands):
        raise ValueError('cfg commands must be printable ASCII without embedded newlines')
    data = ('\n'.join(commands) + '\n').encode('ascii')
    if len(data) >= 16384:
        raise ValueError('cfg exceeds retained command buffer')
    destination = game / name
    temporary = game / (name + '.next')
    temporary.write_bytes(data)
    deadline = time.monotonic() + 3
    try:
        while True:
            try:
                os.replace(temporary, destination)
                if destination.read_bytes() != data:
                    raise OSError('atomic cfg replacement differs')
                return
            except PermissionError:
                if time.monotonic() >= deadline:
                    raise
                time.sleep(0.01)
    finally:
        temporary.unlink(missing_ok=True)


def correlate_callbacks(lines):
    auth_lines = [line for line in lines if line.startswith('CSE_FIXTURE_AUTH ')]
    if len(auth_lines) != 1:
        raise ValueError('phase needs exactly one fresh authentication callback')
    match = re.fullmatch(r'CSE_FIXTURE_AUTH id=(\d+) steam_authorized=1', auth_lines[0])
    if not match:
        raise ValueError('phase is not Steam-authorized')
    player = int(match[1])
    required = [rf'CSE_FIXTURE_STARTED id={player}', rf'CSE_FIXTURE_MENU_REPLY id={player} item=0',
                rf'CSE_FIXTURE_CVAR_REPLY id={player} name=cl_updaterate value=37(?:\.0*)?',
                rf'CSE_FIXTURE_CLIENT_ACK id={player}']
    if any(not any(re.fullmatch(pattern, line) for line in lines) for pattern in required):
        raise ValueError('phase callbacks do not correlate with this authorized client')
    return player


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def save(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def wait_stage_marker(read_console, marker, ensure_live, timeout=15):
    deadline = time.monotonic() + timeout
    while marker not in read_console():
        ensure_live()
        if time.monotonic() >= deadline:
            raise TimeoutError('client did not consume stage: ' + marker)
        time.sleep(0.01)


class FixtureLog:
    def __init__(self, folder):
        self.folder = folder
        self.offsets = {path: path.stat().st_size for path in folder.glob('L*.log')}

    def lines(self):
        result = []
        for path in sorted(self.folder.glob('L*.log')):
            offset = self.offsets.get(path, 0)
            with path.open('rb') as stream:
                if path.stat().st_size < offset:
                    raise ValueError('owned fixture log truncated')
                stream.seek(offset)
                for line in stream.read().decode('utf-8', errors='replace').splitlines():
                    match = re.search(r'(CSE_FIXTURE_[A-Z_]+(?: [^\r\n]*)?)', line)
                    if match:
                        result.append(match[1])
        return result


def boot_commands(port):
    commands = ['developer 1', 'cl_showmessages 1', 'fps_max 100', 'gl_vsync 0', 'cl_allowdownload 1',
                'cl_download_ingame 1', 'cl_updaterate 37', 'name CSE_Reconnect_V11', 'alias cse_r_noop ""']
    for phase in range(1, 4):
        prefix = f'cse_r_{phase}'
        actions = {'join': 'jointeam 2; joinclass 1; spec_pos', 'request': 'cse_fixture_start',
                   'menu': 'menuselect 1', 'ack': 'cse_fixture_ack; status', 'move': '+forward; +right',
                   'fire': '-forward; -right; +attack',
                   'stop': f'-forward; -right; -attack; stop; echo CSE_RECONNECT_STOP_{phase}'}
        if phase == 2:
            actions['disconnect'] = 'disconnect; echo CSE_RECONNECT_DISCONNECTED'
            actions['connect'] = f'connect 127.0.0.1:{port}; echo CSE_RECONNECT_CONNECTED_COMMAND'
        for name, action in actions.items():
            marker = f'CSE_RECONNECT_STAGE_{phase}_{name}'
            commands.append(f'alias {prefix}_{name} "alias {prefix}_{name} cse_r_noop; {action}; echo {marker}"')
    commands.extend(['alias cse_r_poll "exec cse_reconnect_tick.cfg; wait; cse_r_poll"',
                     f'connect 127.0.0.1:{port}', 'cse_r_poll'])
    return commands


def run(args):
    sys.path.insert(0, str(args.decoder_directory))
    import analyze_external_server_probe as decoder
    sys.path.insert(0, str(args.console_directory))
    from server_lifecycle_probe import RetainedConsoleProcess, await_text

    expected = {item['relative'].lower(): item for item in json.loads(args.identities.read_text(encoding='utf-8-sig'))}
    server_pins = {item['relative'].lower(): item for item in json.loads(args.server_pins.read_text(encoding='utf-8-sig'))}
    result = {'success': False, 'configuration': args.configuration, 'scope': 'selected secure Win32 server reconnect/changelevel',
              'phases': [], 'python_pid': os.getpid(), 'server_port': 27443, 'steam_port': 27444}
    game = args.client / 'cstrike'
    client = None
    server = None

    def console():
        return '\n'.join(path.read_text(errors='replace') for path in
                         [args.client / 'qconsole.log', game / 'qconsole.log'] if path.is_file())

    def stage(phase, name):
        value = f'record cse_reconnect_{phase}' if name == 'ready' else f'cse_r_{phase}_{name}'
        write_cfg(game, 'cse_reconnect_tick.cfg', [value])
        if name != 'ready':
            wait_stage_marker(console, f'CSE_RECONNECT_STAGE_{phase}_{name}', ensure_live)

    def ensure_live():
        if client is not None and client.poll() is not None:
            raise RuntimeError(f'client exited {client.returncode}')
        if server is not None and server.poll() is not None:
            raise RuntimeError(f'server exited {server.poll()}')

    def await_condition(name, condition, timeout=40):
        deadline = time.monotonic() + timeout
        while True:
            if condition():
                return
            if client is not None and client.poll() is not None:
                raise RuntimeError(f'{name}: client exited {client.returncode}')
            if server is not None and server.poll() is not None:
                raise RuntimeError(f'{name}: server exited {server.poll()}')
            if time.monotonic() >= deadline:
                raise TimeoutError(name)
            time.sleep(0.1)

    def check_live(phase, mode):
        path = game / f'cse_reconnect_{phase}.dem'
        if not path.is_file():
            return False
        data = path.read_bytes()
        try:
            if mode in ('messages', 'ack'):
                return decoder.live_message_state(data, mode == 'ack')
            if mode != 'movement':
                return decoder.live_state(data)
            records = [r for r in decoder.demo_records(data, live=True)
                       if r['kind'] == 1 and r['payload'] and r['health'] > 0]
            moving = [r for r in records if any(value != 0 for value in r['movement'])]
            return len(records) >= 100 and len(moving) >= 2 and max(
                max(r['origin'][axis] for r in moving) - min(r['origin'][axis] for r in moving)
                for axis in range(3)) > 4
        except (ValueError, struct.error):
            return False

    def collect_modules(pid, base, wanted, name):
        raw = args.evidence / (name + '-modules.txt')
        subprocess.run([str(args.module_collector), str(pid), str(raw)], check=True, capture_output=True,
                       creationflags=subprocess.CREATE_NO_WINDOW)
        rows = []
        for line in raw.read_text(encoding='utf-8-sig').splitlines():
            path = Path(line)
            try:
                relative = path.relative_to(base).as_posix()
            except ValueError:
                continue
            if relative.lower() not in wanted or digest(path) != wanted[relative.lower()]['sha256']:
                raise ValueError('unrecognized or changed owned loaded module: ' + relative)
            data = path.read_bytes()
            machine = struct.unpack_from('<H', data, struct.unpack_from('<I', data, 0x3c)[0] + 4)[0]
            target = args.evidence / 'images' / ('server' if base == args.server else 'client') / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, target)
            rows.append({'relative': relative, 'sha256': digest(path), 'bytes': len(data), 'machine': machine})
        save(args.evidence / (name + '-modules.json'), rows)
        return rows

    def query_server(marker, condition):
        server.send(['echo ' + marker, 'status'])
        await_text(server, lambda text: marker in text and condition(text[text.rfind(marker):]),
                   'owned server status ' + marker, timeout=15)

    def phase(number, expected_map, log):
        demo_path = game / f'cse_reconnect_{number}.dem'
        stage(number, 'ready')
        await_condition(f'phase{number} actual recording', lambda: demo_path.is_file() and
                        demo_path.stat().st_size >= decoder.HEADER.size and
                        f'recording to cse_reconnect_{number}.dem.' in console(), 120)
        rows = collect_modules(client.pid, args.client, expected, f'phase{number}-client')
        critical = {'hl.exe', 'hw.dll', 'filesystem_stdio.dll', 'vgui.dll', 'vgui2.dll', 'tier0.dll', 'core.dll',
                    'demoplayer.dll', 'sdl3.dll', 'mpg123.dll', 'steam_api64.dll', 'cstrike/cl_dlls/client.dll',
                    'valve/cl_dlls/gameui.dll', 'valve/cl_dlls/particleman.dll'}
        if any(row['machine'] != 0x8664 for row in rows):
            raise ValueError('client loaded non-x64 owned module')
        if not critical.issubset({row['relative'].lower() for row in rows}):
            raise ValueError('phase is missing exact14 native client images')
        if any(row['relative'].lower() == 'cstrike/dlls/mp.dll' for row in rows):
            raise ValueError('remote client loaded own server DLL')
        stage(number, 'join')
        await_condition(f'phase{number} alive', lambda: check_live(number, 'alive'))
        await_condition(f'phase{number} fresh Steam auth', lambda: any(
            re.fullmatch(r'CSE_FIXTURE_AUTH id=\d+ steam_authorized=1', line) for line in log.lines()))
        stage(number, 'request')
        await_condition(f'phase{number} received messages', lambda: check_live(number, 'messages'))
        stage(number, 'menu')
        await_condition(f'phase{number} menu reply', lambda: any(
            re.fullmatch(r'CSE_FIXTURE_MENU_REPLY id=\d+ item=0', line) for line in log.lines()))
        stage(number, 'ack')
        await_condition(f'phase{number} exact ack and cvar', lambda: check_live(number, 'ack') and any(
            re.fullmatch(r'CSE_FIXTURE_CVAR_REPLY id=\d+ name=cl_updaterate value=37(?:\.0*)?', line)
            for line in log.lines()))
        stage(number, 'move')
        await_condition(f'phase{number} actual movement', lambda: check_live(number, 'movement'))
        stage(number, 'fire')
        time.sleep(0.7)
        stage(number, 'stop')
        await_condition(f'phase{number} finalized record', lambda: f'CSE_RECONNECT_STOP_{number}' in console())
        lines = log.lines()
        player = correlate_callbacks(lines)
        data = demo_path.read_bytes()
        analysis = decoder.analyze_messages(data, console(), player)
        if analysis['recorded_state']['map'] != expected_map:
            raise ValueError('new phase retained wrong map')
        samples = [r for r in decoder.demo_records(data) if r['kind'] == 1 and r['payload'] and r['health'] > 0
                   and any(value != 0 for value in r['movement'])]
        span = [max(r['origin'][axis] for r in samples) - min(r['origin'][axis] for r in samples) for axis in range(3)]
        if len(samples) < 2 or max(span) <= 4:
            raise ValueError('no movement within alive nonzero-command samples')
        directory = decoder.HEADER.unpack_from(data)[-1]
        count = struct.unpack_from('<i', data, directory)[0]
        if directory + 4 + count * decoder.ENTRY.size != len(data):
            raise ValueError('phase has trailing demo bytes')
        shutil.copyfile(demo_path, args.evidence / f'phase{number}.dem')
        (args.evidence / f'phase{number}-fixture-delta.log').write_text('\n'.join(lines) + '\n')
        save(args.evidence / f'phase{number}-analysis.json', analysis)
        value = {'phase': number, 'map': expected_map, 'client_pid': client.pid, 'steam_authorized': True,
                 'player_id': player, 'live_client_modules': rows, 'analysis': analysis,
                 'movement_window': {'samples': len(samples), 'span': span}, 'full_eof_trailing_bytes': 0}
        result['phases'].append(value)
        save(args.evidence / 'progress.json', result)

    try:
        command = [args.server / 'hlds.exe', '-console', '-condebug', '-game', 'cstrike', '+ip', '127.0.0.1',
                   '-port', '27443', '-sport', '27444', '-nomaster', '+maxplayers', '4', '+map', 'de_dust2',
                   '+meta', 'list', '+amxx', 'plugins']
        result['server_command'] = [str(value) for value in command]
        server = RetainedConsoleProcess(command, args.server, args.evidence / 'server-console.snapshot',
                                        [args.server / 'qconsole.log', args.server / 'cstrike/qconsole.log'])
        result['server_pid'] = server.pid
        result['server_session'] = server.session_id
        save(args.evidence / 'progress.json', result)
        await_text(server, lambda text: re.search(r'map\s*:\s*de_dust2\b', text) is not None,
                   'owned clone initial boot/status', commands=['status'], timeout=60)
        server.send(['sv_lan', 'mp_consistency'])
        await_text(server, lambda text: '"sv_lan" is "0"' in text and '"mp_consistency" is "1"' in text,
                   'unchanged secure/consistency cvars')
        result['server_modules'] = collect_modules(server.pid, args.server, server_pins, 'server')
        required = {'hlds.exe', 'swds.dll', 'cstrike/dlls/mp.dll', 'cstrike/addons/metamod/metamod.dll',
                    'cstrike/addons/amxmodx/dlls/amxmodx_mm.dll'}
        if not required.issubset({row['relative'].lower() for row in result['server_modules']}):
            raise ValueError('own Win32 server missing required modules')
        if any(row['machine'] != 0x14c for row in result['server_modules']):
            raise ValueError('server clone loaded non-x86 owned module')
        logs = args.server / 'cstrike/addons/amxmodx/logs'
        first_log = FixtureLog(logs)
        write_cfg(game, 'cse_reconnect_boot.cfg', boot_commands(27443))
        stage(1, 'ready')
        client_command = [str(args.client / 'hl.exe'), '-steam', '-game', 'cstrike', '-gl', '-windowed', '-forceres',
                          '-w', '640', '-h', '480', '-novid', '-dev', '-console', '-condebug', '-nomaster',
                          '+exec', 'cse_reconnect_boot.cfg']
        client = subprocess.Popen(client_command, cwd=args.client)
        result['client_pid'] = client.pid
        result['client_command'] = client_command
        phase(1, 'de_dust2', first_log)
        second_log = FixtureLog(logs)
        stage(2, 'disconnect')
        await_condition('actual disconnect command', lambda: 'CSE_RECONNECT_DISCONNECTED' in console())
        query_server('CSE_RECONNECT_ZERO_CLIENTS', lambda text: re.search(r'players\s*:\s*0 active', text) is not None)
        stage(2, 'connect')
        await_condition('actual fresh connect command', lambda: 'CSE_RECONNECT_CONNECTED_COMMAND' in console())
        phase(2, 'de_dust2', second_log)
        third_log = FixtureLog(logs)
        generation = console().count('Serverinfo packet received.')
        write_cfg(game, 'cse_reconnect_tick.cfg', ['cse_r_noop'])
        server.send(['changelevel de_dust'])
        query_server('CSE_RECONNECT_CHANGED_MAP', lambda text: re.search(r'map\s*:\s*de_dust\b', text) is not None)
        await_condition('new client serverinfo generation', lambda: console().count('Serverinfo packet received.') > generation)
        phase(3, 'de_dust', third_log)
        write_cfg(game, 'cse_reconnect_tick.cfg', ['disconnect; quit'])
        result['client_exit_code'] = client.wait(20)
        result['downloads'] = []
        for suffix in ['sound/cse_fixture/probe.wav', 'cse_fixture/probe.txt']:
            paths = [path for path in args.client.rglob('*') if path.is_file()
                     and path.as_posix().lower().endswith('/' + suffix)]
            if not paths or any(digest(path) != server_pins['cstrike/' + suffix]['sha256'] for path in paths):
                raise ValueError('actual downloaded resource differs: ' + suffix)
            result['downloads'].extend({'relative': path.relative_to(args.client).as_posix(),
                                        'sha256': digest(path), 'bytes': path.stat().st_size} for path in paths)
        server.send(['quit'])
        result['server_exit_code'] = server.wait(20)
        if result['client_exit_code'] or result['server_exit_code']:
            raise ValueError('nonzero final graceful exit')
        result['success'] = True
    except Exception as error:
        result['failure'] = f'{type(error).__name__}: {error}'
    finally:
        if client is not None and client.poll() is None:
            try:
                write_cfg(game, 'cse_reconnect_tick.cfg', ['disconnect; quit'])
                client.wait(5)
            except (OSError, subprocess.TimeoutExpired):
                client.kill()
                client.wait(5)
                result['forced_client_cleanup'] = True
        if client is not None:
            result['client_cleanup_exit_code'] = client.returncode
        if server is not None:
            server.close()
            result['forced_server_cleanup'] = server.forced
            result['server_cleanup_exit_code'] = server.exit_code
            (args.evidence / 'server-console.txt').write_text(server.text())
        (args.evidence / 'client-console.txt').write_text(console())
        for path in game.glob('cse_reconnect_*.dem'):
            shutil.copyfile(path, args.evidence / ('retained-' + path.name))
        save(args.evidence / 'result.json', result)
    return 0 if result['success'] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['server', 'client', 'evidence', 'identities', 'server-pins', 'module-collector',
                 'decoder-directory', 'console-directory']:
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--configuration', choices=('Release', 'Debug'), required=True)
    return run(parser.parse_args())


if __name__ == '__main__':
    raise SystemExit(main())
