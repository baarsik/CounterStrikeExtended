"""Owned native HLTV broadcast production and maintained-reader qualification."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import time


def trusted_clock_bound(console, filename):
    if not isinstance(console, str) or not isinstance(filename, str) or not re.fullmatch(r'[A-Za-z0-9_.-]+\.dem', filename):
        raise ValueError('invalid paired producer clock evidence')
    starts = list(re.finditer(r'^Start recording to ' + re.escape(filename) + r'\.$', console, re.M))
    completes = list(re.finditer(r'^Completed demo ' + re.escape(filename) + r'\.$', console, re.M))
    if len(starts) != 1 or len(completes) != 1 or starts[0].end() >= completes[0].start():
        raise ValueError('clock evidence lacks unique ordered record/complete events')
    status = console[starts[0].end():completes[0].start()]
    match = re.search(r'^--- HLTV Status ---\nOnline (\d{2}):(\d{2}),[^\n]*\n', status, re.M)
    if not match or int(match[2]) >= 60:
        raise ValueError('missing trusted post-start Online status')
    if not re.search(r'^Recording to ' + re.escape(filename) + r', Length [0-9.]+ sec\.$', status[match.end():], re.M):
        raise ValueError('clock status is not paired with this active recording')
    upper = int(match[1]) * 60 + int(match[2]) + 1
    maximum_clock_upper_seconds = 600  # Bounded scheduled producer lifetime, not a general-file clock claim.
    if not 1 <= upper <= maximum_clock_upper_seconds:
        raise ValueError('clock evidence exceeds the bounded producer lifetime')
    encoded = struct.unpack('<I', struct.pack('<f', upper))[0]
    previous = struct.unpack('<f', struct.pack('<I', encoded - 1))[0]
    bound = (upper - previous) / 2
    return {'upper_seconds': upper, 'bound_seconds': bound,
            'bound_bits': f"{struct.unpack('<I', struct.pack('<f', bound))[0]:08x}",
            'online_line': match[0].splitlines()[1], 'filename': filename,
            'normalized_console_sha256': hashlib.sha256(console.encode('utf-8')).hexdigest(),
            'scope': 'trusted paired fixture status; consistency with tested binary32 RNE model',
            'production_thread_fp_state_measured': False}


def validate_broadcast(data, marker, decoder, clock_evidence=None):
    if not isinstance(clock_evidence, dict) or set(clock_evidence) != {'console', 'filename', 'demo_sha256'}:
        raise ValueError('a paired controlled producer clock receipt is required')
    file_sha = hashlib.sha256(data).hexdigest()
    if clock_evidence['demo_sha256'] != file_sha:
        raise ValueError('clock receipt identifies different producer bytes')
    clock = trusted_clock_bound(clock_evidence['console'], clock_evidence['filename'])
    if len(data) < decoder.HEADER.size:
        raise ValueError('truncated HLTV header')
    stamp, protocol, network, map_raw, game_raw, map_crc, directory = decoder.HEADER.unpack_from(data)
    if stamp != b'HLDEMO\0\0' or (protocol, network) != (5, 48):
        raise ValueError('invalid HLTV header/protocol')
    map_name = map_raw.split(b'\0', 1)[0]
    game_name = game_raw.split(b'\0', 1)[0]
    if map_name != b'de_dust2' or game_name != b'cstrike' or map_crc != 0:
        raise ValueError('unexpected controlled HLTV world')
    if not decoder.HEADER.size <= directory <= len(data) - 4:
        raise ValueError('unfinished HLTV directory')
    count = struct.unpack_from('<i', data, directory)[0]
    if count != 2 or directory + 4 + count * decoder.ENTRY.size != len(data):
        raise ValueError('HLTV directory does not end at EOF')
    entries = [decoder.ENTRY.unpack_from(data, directory + 4 + index * decoder.ENTRY.size) for index in range(count)]
    extents = [[entry[6], entry[6] + entry[7]] for entry in entries]
    if [entry[0] for entry in entries] != [0, 1] or extents[0][0] != decoder.HEADER.size or \
            extents[0][1] != extents[1][0] or extents[1][1] != directory:
        raise ValueError('HLTV tracks do not consume the complete frame region')
    tracks = []
    network_records = []
    for entry, extent in zip(entries, extents):
        if not math.isfinite(entry[4]) or entry[4] < 0 or entry[5] < 0 or entry[7] < decoder.COMMAND.size:
            raise ValueError('invalid HLTV directory time/count/extent')
        offset, end = extent
        records = []
        while offset < end:
            begin = offset
            if offset + decoder.COMMAND.size > end:
                raise ValueError('truncated HLTV command')
            kind, time, frame = decoder.COMMAND.unpack_from(data, offset)
            bits = struct.unpack_from('<I', data, offset + 1)[0]
            if not math.isfinite(time) or frame < 0:
                raise ValueError('nonfinite HLTV timestamp or negative frame')
            offset += decoder.COMMAND.size
            value = {'kind': kind, 'time': time, 'bits': bits, 'frame': frame, 'offset': begin}
            if kind in (0, 1):
                if offset + decoder.DEMO_INFO_BYTES + decoder.SEQUENCE.size > end:
                    raise ValueError('truncated legacy HLTV frame')
                info = data[offset:offset + decoder.DEMO_INFO_BYTES]
                if info != bytes(decoder.DEMO_INFO_BYTES):
                    raise ValueError('selected live HLTV writer requires its zeroDemoInfo')
                offset += decoder.DEMO_INFO_BYTES
                sequence = decoder.SEQUENCE.unpack_from(data, offset)
                offset += decoder.SEQUENCE.size
                length = sequence[-1]
                if not 0 <= length <= 65536 or offset + length > end:
                    raise ValueError('invalid HLTV network message length')
                value.update(payload=data[offset:offset + length], sequence=sequence[0])
                offset += length
                if kind == 1:
                    network_records.append(value)
            elif kind in (2, 5):
                pass
            elif kind in decoder.FIXED_PAYLOAD_BYTES:
                length = decoder.FIXED_PAYLOAD_BYTES[kind]
                if offset + length > end:
                    raise ValueError('truncated fixed HLTV record')
                if kind == 4:
                    floats = struct.unpack_from('<6f', data, offset) + struct.unpack_from('<f', data, offset + 28)
                    if not all(math.isfinite(value) for value in floats):
                        raise ValueError('nonfinite HLTV client data')
                offset += length
            elif kind == 8:
                if offset + 8 > end:
                    raise ValueError('truncated HLTV sound record')
                sample_bytes = struct.unpack_from('<i', data, offset + 4)[0]
                if not 0 <= sample_bytes <= 65536:
                    raise ValueError('invalid HLTV sound byte count')
                offset += 8 + sample_bytes + 16
            elif kind == 9:
                if offset + 4 > end:
                    raise ValueError('truncated HLTV client record')
                length = struct.unpack_from('<i', data, offset)[0]
                if not 0 <= length <= 65536:
                    raise ValueError('invalid HLTV client byte count')
                offset += 4 + length
            else:
                raise ValueError('unknown HLTV command')
            if offset > end or (kind == 5 and offset != end):
                raise ValueError('record beyond extent or bytes after STOP')
            records.append(value)
        if not records or records[-1]['kind'] != 5:
            raise ValueError('HLTV track missing STOP')
        tracks.append(records)
    if len(tracks[0]) != 2 or len(tracks[1]) < 4:
        raise ValueError('wrong initial HLTV tick shape')
    prefix = tracks[0] + tracks[1][:3]
    if [(record['kind'], record['frame']) for record in prefix] != [(0, 0), (5, 0), (2, 0), (1, 1), (4, 1)]:
        raise ValueError('wrong initial HLTV tick order/frame identities')
    initial = prefix[0]
    if any(record['bits'] != initial['bits'] for record in prefix) or abs(initial['time']) > clock['bound_seconds']:
        raise ValueError('unmatched or out-of-rounding-bound initial HLTV timestamps')
    previous = initial['time']
    for record in tracks[1][3:]:
        if record['time'] < 0 or record['time'] < previous:
            raise ValueError('negative outside initial tick or regressing HLTV timestamp')
        previous = record['time']
    if entries[0][4] != 0 or entries[0][5] != 0:
        raise ValueError('invalid loading-track accounting')
    if entries[1][4] != tracks[1][-1]['time'] or entries[1][5] != len(network_records) or \
            tracks[1][-1]['frame'] != len(network_records):
        raise ValueError('HLTV duration/frame/STOP accounting differs')
    if [record['frame'] for record in network_records] != list(range(1, len(network_records) + 1)):
        raise ValueError('HLTV network frame identities are not contiguous')
    nonempty = [record for record in network_records if record['payload']]
    if len(nonempty) < 100 or max(record['time'] for record in nonempty) - min(record['time'] for record in nonempty) < 4:
        raise ValueError('insufficient actual HLTV timeline/frame window')
    marker_records = []
    expected_marker = marker.encode('ascii') + b'\0'
    for record in nonempty:
        payload = record['payload']
        begin = 0
        while True:
            # Proxy::WriteHUDMsg uses DirectorCmd::SetMessageData/WriteToStream:
            # svc_director, byte payload length, DRC_CMD_MESSAGE, effect, color,
            # six binary32 values, then the complete NUL-terminated text.
            offset = payload.find(bytes([51]), begin)
            if offset < 0:
                break
            begin = offset + 1
            director_header_bytes = 32
            if offset + director_header_bytes > len(payload):
                continue
            end = offset + 2 + payload[offset + 1]
            effect = payload[offset + 3]
            text = offset + director_header_bytes
            values = struct.unpack_from('<6f', payload, offset + 8)
            if payload[offset + 2] == 6 and effect in (0, 1, 2) and end <= len(payload) and \
                    all(math.isfinite(value) for value in values) and \
                    payload[text:end] == expected_marker:
                marker_records.append({'frame': record['frame'], 'sequence': record['sequence'], 'time': record['time'],
                                       'effect': effect, 'encoding': 'svc_director/DRC_CMD_MESSAGE',
                                       'command_bytes': end - offset})
    if not marker_records:
        raise ValueError('missing exact encoded HLTV textmessage marker')
    state = {'map': 'de_dust2', 'game_directory': 'cstrike', 'map_crc': map_crc, 'sha256': file_sha,
             'bytes': len(data), 'tracks': 2, 'network_frames': len(network_records),
             'nonempty_network_frames': len(nonempty), 'distinct_incoming_sequences': len({r['sequence'] for r in nonempty}),
             'duration_seconds': entries[1][4],
             'network_sample_duration_seconds': max(r['time'] for r in nonempty) - min(r['time'] for r in nonempty),
             'alive_samples': 0, 'scope': 'HLTV broadcast timeline; viewer info is zero, player evidence is separate'}
    return {'recorded_state': state, 'marker_records': marker_records, 'track_extents': extents,
            'full_eof_trailing_bytes': 0, 'clock_bound': clock,
            'initial_tick': {'bits': f"{initial['bits']:08x}", 'time': initial['time'], 'records': 5},
            'profile': 'selected-v11-hltv-initial-tick-binary32-rne-consistency',
            'production_thread_fp_state_measured': False,
            'viewer_state_scope': 'HLTV zeroDemoInfo; no viewer alive/movement requirement'}


def verify_input(path, expected_sha256):
    value = digest(path)
    if value != expected_sha256:
        raise ValueError('input demo identity differs')
    return value


def reader_completed(console):
    return re.search(r'(?s)CSE_HLTV_REPLAY_BEGIN.*Playing demo from cse_hltv_broadcast\.dem.*'
                     r'Host_EndGame:\s*End of demo\..*CSE_HLTV_REPLAY_DONE', console) is not None


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def save(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def run(args):
    sys.path.insert(0, str(args.decoder_directory))
    import analyze_external_server_probe as decoder
    sys.path.insert(0, str(args.reconnect_directory))
    import reconnect_probe as control
    sys.path.insert(0, str(args.console_directory))
    from server_lifecycle_probe import RetainedConsoleProcess, await_text

    expected_rows = json.loads(args.identities.read_text(encoding='utf-8-sig'))
    if args.architecture == 'x86':
        expected_rows = [row for row in expected_rows if row['configuration'] == args.configuration]
    expected = {row['relative'].lower(): row for row in expected_rows}
    result = {'success': False, 'mode': args.mode, 'configuration': args.configuration,
              'architecture': args.architecture, 'python_pid': os.getpid()}
    player = None
    server = None
    hltv = None
    streams = []
    critical_client = {'hl.exe', 'hw.dll', 'filesystem_stdio.dll', 'vgui.dll', 'vgui2.dll', 'tier0.dll', 'core.dll',
                       'demoplayer.dll', 'sdl3.dll', 'mpg123.dll', 'cstrike/cl_dlls/client.dll',
                       'valve/cl_dlls/gameui.dll', 'valve/cl_dlls/particleman.dll'}
    if args.architecture == 'x64':
        critical_client.add('steam_api64.dll')

    def client_console():
        return '\n'.join(path.read_text(errors='replace') for path in
                         [args.client / 'qconsole.log', args.client / 'cstrike/qconsole.log'] if path.is_file())

    def check_alive():
        for name, process in [('player', player), ('server', server), ('hltv', hltv)]:
            if process is not None and process.poll() is not None:
                raise RuntimeError(name + ' exited early: ' + str(process.poll()))

    def wait_for(name, condition, timeout=45):
        deadline = time.monotonic() + timeout
        while not condition():
            check_alive()
            if time.monotonic() >= deadline:
                raise TimeoutError(name)
            time.sleep(0.1)

    def launch_client(cfg):
        command = [str(args.client / 'hl.exe'), '-steam', '-game', 'cstrike', '-gl', '-windowed', '-forceres',
                   '-w', '640', '-h', '480', '-novid', '-dev', '-console', '-condebug', '-nomaster', '+exec', cfg]
        output = (args.evidence / 'client-stdout.log').open('wb')
        errors = (args.evidence / 'client-stderr.log').open('wb')
        streams.extend([output, errors])
        result['client_command'] = command
        process = subprocess.Popen(command, cwd=args.client, stdout=output, stderr=errors)
        result['client_pid'] = process.pid
        return process

    def modules(pid, base, wanted, tag, required, machine, legacy=False):
        raw = args.evidence / (tag + '-modules.txt')
        subprocess.run([str(args.module_collector), str(pid), str(raw)], check=True, capture_output=True,
                       creationflags=subprocess.CREATE_NO_WINDOW)
        observed = {}
        extras = []
        allowed = {'steam_api.dll', 'chromehtml.dll', 'libcef.dll', 'icudt.dll', 'avcodec-53.dll',
                   'avutil-51.dll', 'avformat-53.dll', 'platform/servers/serverbrowser.dll', 'vstdlib.dll'}
        for line in raw.read_text(encoding='utf-8-sig').splitlines():
            path = Path(line)
            try:
                relative = path.relative_to(base).as_posix()
            except ValueError:
                continue
            key = relative.lower()
            data = path.read_bytes()
            actual_machine = struct.unpack_from('<H', data, struct.unpack_from('<I', data, 0x3c)[0] + 4)[0]
            row = {'relative': relative, 'sha256': digest(path), 'bytes': len(data), 'machine': actual_machine}
            if actual_machine != machine:
                raise ValueError('wrong loaded owned image machine: ' + relative)
            if key not in wanted:
                if not legacy or key not in allowed:
                    raise ValueError('unrecognized loaded owned module: ' + relative)
                extras.append(row)
            else:
                if row['sha256'] != wanted[key]['sha256']:
                    raise ValueError('loaded image differs: ' + relative)
                observed[key] = row
            target = args.evidence / 'images' / tag / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, target)
        if not required.issubset(observed):
            raise ValueError('missing live required images: ' + ','.join(sorted(required - observed.keys())))
        if 'cstrike/dlls/mp.dll' in observed and tag in ('player', 'reader'):
            raise ValueError('remote/demo client loaded server DLL')
        report = {'matched': list(observed.values()), 'legacy_provider_observations': extras}
        save(args.evidence / (tag + '-modules.json'), report)
        return report

    try:
        if args.mode == 'reader':
            verify_input(args.input_demo, args.input_sha256)
            copied = args.client / 'cstrike/cse_hltv_broadcast.dem'
            verify_input(copied, args.input_sha256)
            control.write_cfg(args.client / 'cstrike', 'cse_hltv_replay.cfg',
                              ['developer 1', 'fps_max 100', 'gl_vsync 0', 'echo CSE_HLTV_REPLAY_BEGIN',
                               'playdemo cse_hltv_broadcast'] + ['wait'] * 1800 +
                              ['echo CSE_HLTV_REPLAY_DONE', 'quit'])
            player = launch_client('cse_hltv_replay.cfg')
            started = time.monotonic()
            collected = False
            while player.poll() is None:
                text = client_console()
                if not collected and time.monotonic() - started >= 3 and 'Playing demo from cse_hltv_broadcast.dem' in text:
                    result['reader_modules'] = modules(player.pid, args.client, expected, 'reader', critical_client,
                                                       0x8664 if args.architecture == 'x64' else 0x14c,
                                                       legacy=args.architecture == 'x86')
                    collected = True
                if time.monotonic() - started > 90:
                    raise TimeoutError('actual maintained-reader replay')
                time.sleep(0.1)
            result['client_exit_code'] = player.returncode
            if player.returncode != 0 or not collected or not reader_completed(client_console()):
                raise ValueError('reader did not reach actual full EOF and clean exit')
            result['input_sha256'] = verify_input(copied, args.input_sha256)
            result['replay_reached_end'] = True
        else:
            pins = {row['relative'].lower(): row for row in json.loads(args.server_pins.read_text(encoding='utf-8-sig'))}
            server_command = [args.server / 'hlds.exe', '-console', '-condebug', '-game', 'cstrike', '+ip', '127.0.0.1',
                              '-port', '27443', '-sport', '27444', '-nomaster', '+maxplayers', '4', '+map', 'de_dust2',
                              '+meta', 'list', '+amxx', 'plugins']
            result['server_command'] = [str(value) for value in server_command]
            server = RetainedConsoleProcess(server_command, args.server, args.evidence / 'server-console.snapshot',
                                            [args.server / 'qconsole.log', args.server / 'cstrike/qconsole.log'])
            result['server_pid'] = server.pid
            result['server_session'] = server.session_id
            save(args.evidence / 'progress.json', result)
            await_text(server, lambda text: re.search(r'map\s*:\s*de_dust2\b', text) is not None,
                       'unchanged secure clone initial map', commands=['status'], timeout=60)
            server.send(['sv_lan', 'mp_consistency'])
            await_text(server, lambda text: '"sv_lan" is "0"' in text and '"mp_consistency" is "1"' in text,
                       'clone original security/consistency settings')
            result['server_modules'] = modules(server.pid, args.server, pins, 'server',
                {'hlds.exe', 'swds.dll', 'cstrike/dlls/mp.dll', 'cstrike/addons/metamod/metamod.dll',
                 'cstrike/addons/amxmodx/dlls/amxmodx_mm.dll'}, 0x14c)
            log = control.FixtureLog(args.server / 'cstrike/addons/amxmodx/logs')
            game = args.client / 'cstrike'
            control.write_cfg(game, 'cse_reconnect_boot.cfg', control.boot_commands(27443)[:-2] +
                ['alias cse_hltv_move "alias cse_hltv_move cse_r_noop; +forward; +right; echo CSE_HLTV_PLAYER_MOVING"'] +
                control.boot_commands(27443)[-2:])
            control.write_cfg(game, 'cse_reconnect_tick.cfg', ['record cse_reconnect_1'])
            player = launch_client('cse_reconnect_boot.cfg')
            player_demo = game / 'cse_reconnect_1.dem'
            wait_for('actual player recording', lambda: player_demo.is_file() and player_demo.stat().st_size >= 544 and
                     'recording to cse_reconnect_1.dem.' in client_console(), timeout=120)
            result['player_modules'] = modules(player.pid, args.client, expected, 'player', critical_client, 0x8664)

            def action(name):
                control.write_cfg(game, 'cse_reconnect_tick.cfg', ['cse_r_1_' + name])
                control.wait_stage_marker(client_console, 'CSE_RECONNECT_STAGE_1_' + name, check_alive)

            def live(mode):
                try:
                    data = player_demo.read_bytes()
                    if mode in ('messages', 'ack'):
                        return decoder.live_message_state(data, mode == 'ack')
                    records = [r for r in decoder.demo_records(data, live=True)
                               if r['kind'] == 1 and r['payload'] and r['health'] > 0
                               and any(value != 0 for value in r['movement'])]
                    return len(records) >= 2 and max(max(r['origin'][axis] for r in records) -
                        min(r['origin'][axis] for r in records) for axis in range(3)) > 4
                except (OSError, ValueError, struct.error):
                    return False

            action('join')
            wait_for('fresh actual player Steam authentication', lambda: any(
                re.fullmatch(r'CSE_FIXTURE_AUTH id=\d+ steam_authorized=1', line) for line in log.lines()))
            authorized = [re.fullmatch(r'CSE_FIXTURE_AUTH id=(\d+) steam_authorized=1', line) for line in log.lines()]
            authorized = [match for match in authorized if match]
            if len(authorized) != 1:
                raise ValueError('expected one native player auth before HLTV connection')
            native_player_id = int(authorized[0][1])
            action('request')
            wait_for('received player fixture messages', lambda: live('messages'))
            action('menu')
            wait_for('actual player menu reply', lambda: any(
                re.fullmatch(r'CSE_FIXTURE_MENU_REPLY id=\d+ item=0', line) for line in log.lines()))
            action('ack')
            wait_for('exact received player ack and cvar', lambda: live('ack') and any(
                re.fullmatch(r'CSE_FIXTURE_CVAR_REPLY id=\d+ name=cl_updaterate value=37(?:\.0*)?', line)
                for line in log.lines()))
            action('move')
            wait_for('real player movement before HLTV ready', lambda: live('movement'))
            hltv_command = [args.hltv / 'hltv.exe', '-novgui', '-nodns', '-ip', '127.0.0.1', '-port', '27445',
                            '+unloadmodule', 'master000', '+publicgame', '0', '+autoretry', '1', '+delay', '0',
                            '+logfile', 'cse_hltv_broadcast.txt', '+connect', '127.0.0.1:27443']
            result['hltv_command'] = [str(value) for value in hltv_command]
            hltv = RetainedConsoleProcess(hltv_command, args.hltv, args.evidence / 'hltv-console.snapshot',
                [args.hltv / name / 'cse_hltv_broadcast.txt' for name in ('', 'valve', 'cstrike')])
            result['hltv_pid'] = hltv.pid
            result['hltv_session'] = hltv.session_id
            save(args.evidence / 'progress.json', result)
            await_text(hltv, lambda text: 'Connected to Game Server 127.0.0.1:27443' in text and
                       'Map "de_dust2.bsp"' in text, 'NEW secure Win32 HLTV connection/world', commands=['status'], timeout=60)
            result['hltv_modules'] = modules(hltv.pid, args.hltv, expected, 'hltv',
                {'hltv.exe', 'proxy.dll', 'core.dll', 'filesystem_stdio.dll', 'steam_api64.dll'}, 0x8664)
            control.write_cfg(game, 'cse_reconnect_tick.cfg', ['cse_hltv_move'])
            control.wait_stage_marker(client_console, 'CSE_HLTV_PLAYER_MOVING', check_alive)
            player_time_before = max(r['time'] for r in decoder.demo_records(player_demo.read_bytes(), live=True)
                                     if r['kind'] == 1)
            base = 'cse_hltv_v11_' + args.configuration.lower()
            marker = 'CSE_HLTV_BROADCAST_' + args.configuration.upper()
            hltv.send(['record ' + base, 'status'])
            candidates = lambda: list(args.hltv.rglob(base + '-*.dem'))
            wait_for('actual native HLTV output file', lambda: len(candidates()) == 1 and
                     candidates()[0].stat().st_size >= decoder.HEADER.size)
            output = candidates()[0]
            hltv.send(['localmsg ' + marker])

            def broadcast_ready():
                records = [r for r in decoder.demo_records(output.read_bytes(), live=True) if r['kind'] == 1 and r['payload']]
                return len(records) >= 100 and max(r['time'] for r in records) - min(r['time'] for r in records) >= 4

            wait_for('actual broadcast timeline/frame window', broadcast_ready, timeout=45)
            hltv.send(['stoprecording', 'status'])
            await_text(hltv, lambda text: 'Completed demo ' + output.name + '.' in text,
                       'real HLTV stoprecording finalization', timeout=15)
            original_bytes = output.read_bytes()
            clock_evidence = {'console': hltv.snapshot, 'filename': output.name,
                              'demo_sha256': hashlib.sha256(original_bytes).hexdigest()}
            analysis = validate_broadcast(original_bytes, marker, decoder, clock_evidence)
            shutil.copyfile(output, args.evidence / 'broadcast.dem')
            result['broadcast'] = {'filename': output.name, 'sha256': digest(output), 'analysis': analysis, 'marker': marker}
            action('stop')
            lines = log.lines()
            player_lines = [line for line in lines if re.search(r'\bid=' + str(native_player_id) + r'(?: |$)', line)]
            player_id = control.correlate_callbacks(player_lines)
            player_analysis = decoder.analyze_messages(player_demo.read_bytes(), client_console(), player_id)
            records = [r for r in decoder.demo_records(player_demo.read_bytes())
                       if r['kind'] == 1 and r['payload'] and r['health'] > 0 and any(r['movement']) and r['time'] > player_time_before]
            span = [max(r['origin'][axis] for r in records) - min(r['origin'][axis] for r in records) for axis in range(3)]
            if len(records) < 2 or max(span) <= 4:
                raise ValueError('actual player movement window missing')
            result['active_player'] = {'steam_authorized': True, 'player_id': player_id, 'analysis': player_analysis,
                                       'moving_samples_during_broadcast': len(records), 'movement_span_during_broadcast': span,
                                       'player_demo_time_at_broadcast_start': player_time_before}
            shutil.copyfile(player_demo, args.evidence / 'player.dem')
            (args.evidence / 'player-fixture-delta.log').write_text('\n'.join(player_lines) + '\n')
            (args.evidence / 'all-fixture-delta.log').write_text('\n'.join(lines) + '\n')
            control.write_cfg(game, 'cse_reconnect_tick.cfg', ['disconnect; quit'])
            result['client_exit_code'] = player.wait(20)
            hltv.send(['disconnect', 'quit'])
            result['hltv_exit_code'] = hltv.wait(20)
            server.send(['quit'])
            result['server_exit_code'] = server.wait(20)
            if any(result[key] for key in ('client_exit_code', 'hltv_exit_code', 'server_exit_code')):
                raise ValueError('nonzero producer graceful exit')
        result['success'] = True
    except Exception as error:
        result['failure'] = f'{type(error).__name__}: {error}'
    finally:
        if player is not None and player.poll() is None:
            try:
                tick = 'cse_reconnect_tick.cfg' if args.mode == 'producer' else 'cse_hltv_replay.cfg'
                control.write_cfg(args.client / 'cstrike', tick, ['disconnect; quit'])
                player.wait(5)
            except (OSError, subprocess.TimeoutExpired):
                player.kill()
                player.wait(5)
                result['forced_client_cleanup'] = True
        if player is not None:
            result['client_cleanup_exit_code'] = player.returncode
        for name, process in [('hltv', hltv), ('server', server)]:
            if process is not None:
                process.close()
                result[name + '_forced_cleanup'] = process.forced
                result[name + '_cleanup_exit_code'] = process.exit_code
                (args.evidence / (name + '-console.txt')).write_text(process.text())
        (args.evidence / 'client-console.txt').write_text(client_console())
        if args.mode == 'producer':
            for path in args.hltv.rglob('cse_hltv_v11_*.dem'):
                shutil.copyfile(path, args.evidence / ('retained-' + path.name))
        for stream in streams:
            stream.close()
        save(args.evidence / 'result.json', result)
    return 0 if result['success'] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('producer', 'reader'), required=True)
    parser.add_argument('--configuration', choices=('Release', 'Debug'), required=True)
    parser.add_argument('--architecture', choices=('x86', 'x64'), default='x64')
    for name in ['client', 'evidence', 'identities', 'module-collector', 'decoder-directory', 'console-directory',
                 'reconnect-directory']:
        parser.add_argument('--' + name, type=Path, required=True)
    for name in ['server', 'hltv', 'server-pins', 'input-demo']:
        parser.add_argument('--' + name, type=Path)
    parser.add_argument('--input-sha256')
    return run(parser.parse_args())


if __name__ == '__main__':
    raise SystemExit(main())
