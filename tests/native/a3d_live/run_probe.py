"""Run one bounded dummy or explicitly Root-authorized output case; never set OS routes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time


def parse_options(argv=None, environ=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--graph', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--profile', choices=['Release', 'Debug'], required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--instrumented', action='store_true')
    parser.add_argument('--inject-put-failure', action='store_true')
    live = parser.add_mutually_exclusive_group()
    live.add_argument('--root-authorized-live-default', action='store_true')
    live.add_argument('--root-authorized-live-selected', action='store_true')
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument('--select-device-name')
    selection.add_argument('--select-first-enumerated', action='store_true')
    parser.add_argument('--selected-source-only', action='store_true')
    args = parser.parse_args(argv)
    env = os.environ if environ is None else environ
    selected = args.select_device_name is not None or args.select_first_enumerated
    actual = args.root_authorized_live_default or args.root_authorized_live_selected
    if args.inject_put_failure and not args.instrumented:
        parser.error('--inject-put-failure requires --instrumented')
    if args.selected_source_only and not selected:
        parser.error('--selected-source-only requires public device selection')
    if args.root_authorized_live_default and selected:
        parser.error('default live admission cannot select a device')
    if args.root_authorized_live_selected and args.select_device_name is None:
        parser.error('selected live admission requires the exact --select-device-name')
    if actual and args.select_first_enumerated:
        parser.error('first-enumerated selection is dummy-only')
    if actual and env.get('CSE_ROOT_LIVE_OUTPUT_AUTHORIZED') != '1':
        parser.error('Root live admission is required')
    if actual and env.get('SDL_AUDIO_DRIVER') == 'dummy':
        parser.error('live and dummy evidence must stay distinct')
    return args


def validate_capture_binding(instrumented, fixture, files):
    observer = str(fixture / 'liba3d_live_capture.dylib')
    if instrumented and (observer not in files or len(files[observer]) != 64):
        raise ValueError('instrumented evidence requires the absolute observer path and SHA256')
    if not instrumented and observer in files:
        raise ValueError('bare evidence must not claim an observer binding')


def validate_observation(mode, observed):
    if not observed.get('completed'):
        raise ValueError('probe did not complete all three lifetimes')
    if mode['instrumented'] and not mode['selected_source_only'] and not observed.get('music_pcm_progress'):
        raise ValueError('successful music PCM progress after A3D teardown was not observed')
    if mode.get('selected') and not observed.get('public_guid_selected_api'):
        raise ValueError('public copied-token initialization did not pass all three lifetimes')
    if mode['instrumented'] and mode.get('selected') and not observed.get('explicit_selected_open'):
        raise ValueError('observer did not confirm the full selected label and explicit open')
    if mode['instrumented'] and mode['selected_source_only'] and not observed.get('no_default_opens'):
        raise ValueError('selected-source-only observation lacks the zero-default-open gate')


def hashes(paths):
    return {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}


def main():
    args = parse_options()
    if args.evidence.exists():
        raise ValueError('Never overwrite a prior evidence directory')
    graph = args.graph.resolve()
    fixture = args.fixture.resolve()
    runtime = graph / 'runtime' / args.profile
    binary = fixture / ('a3d_live_instrumented' if args.instrumented else 'a3d_live_bare')
    module = runtime / 'a3dapi.dylib'
    sdl = runtime / 'libSDL3.0.dylib'
    mpg = runtime / 'libmpg123.dylib'
    track = graph / 'dependencies/mpg123/source/mpg123-1.33.7/src/tests/sweep.mp3'
    selected = args.select_device_name is not None or args.select_first_enumerated
    mode = ('--root-authorized-live-selected' if args.root_authorized_live_selected else
            '--root-authorized-live-default' if args.root_authorized_live_default else '--dummy')
    env = dict(os.environ, CMAKE_BUILD_PARALLEL_LEVEL='1', CTEST_PARALLEL_LEVEL='1')
    if mode == '--dummy':
        env['SDL_AUDIO_DRIVER'] = 'dummy'
    command = [str(binary), mode, str(module), str(sdl), str(track), str(args.evidence.resolve())]
    if args.select_device_name is not None:
        command += ['--select-device-name', args.select_device_name]
    for option in ['select_first_enumerated', 'selected_source_only', 'inject_put_failure']:
        if getattr(args, option):
            command.append('--' + option.replace('_', '-'))
    paths = [binary, module, sdl, mpg, track]
    if args.instrumented:
        paths.append(fixture / 'liba3d_live_capture.dylib')
    before = hashes(paths)
    validate_capture_binding(args.instrumented, fixture, before)
    source_root = Path(__file__).resolve().parents[3]
    source_paths = [source_root / 'tests/native/a3d_live' / name for name in
                    ['live_probe.cpp', 'capture.cpp', 'capture.h', 'run_probe.py', 'prepare.py', 'CMakeLists.txt']]
    source_paths += [source_root / 'tests/native/a3d_module' / name for name in
                     ['enumeration_caller.h', 'enumeration_admission.cpp', 'caller_interfaces.h']]
    source_paths += [graph / 'compile_commands.json', fixture / 'production_dma.inc',
                     fixture / 'production_dma_record.h']
    source_before = hashes(source_paths)
    args.evidence.mkdir(parents=True)
    start = time.monotonic()
    with (args.evidence / 'probe.log').open('w') as log:
        try:
            process = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, env=env, timeout=20)
            code = process.returncode
        except subprocess.TimeoutExpired:
            code = 124
            log.write('\nTIMEOUT: bounded process terminated; no automatic retry or device intervention\n')
    elapsed = time.monotonic() - start
    after = hashes(paths)
    source_after = hashes(source_paths)
    log = (args.evidence / 'probe.log').read_text()
    selections = [json.loads(line.removeprefix('selection_json=')) for line in log.splitlines()
                  if line.startswith('selection_json=')]
    observed = {
        'completed': 'PASS cycles=3 ' in log and log.count('unload_absent=1 owner_shutdown_balanced=1') == 3,
        'public_guid_selected_api': log.count('public_guid_selected_api=PASS') == 3,
        'explicit_selected_open': log.count('selected_open stage=provider explicit=1') == 3 and
                                 log.count('selected_open stage=source explicit=1') == 3,
        'music_pcm_progress': log.count('owners stage=after_a3d_teardown dma_progress=1 music_state=playing '
                                       'music_pcm_progress=PASS') == 2,
        'no_default_opens': 'selected_source_only explicit_opens=6 default_opens=0 dma_music_owners=UNOPENED' in log,
    }
    validation_error = None
    if code == 0:
        try:
            validate_observation({'instrumented': args.instrumented, 'selected': selected,
                                  'selected_source_only': args.selected_source_only}, observed)
            if selected and (len(selections) != 3 or
                             any(item['label'] != selections[0]['label'] for item in selections)):
                raise ValueError('copied selected label changed or was missing')
            if args.select_device_name is not None and selections[0]['label'] != args.select_device_name:
                raise ValueError('copied selected label does not exactly match the requested label')
            if before != after or source_before != source_after:
                raise ValueError('bound image or source hashes changed during the case')
            validate_capture_binding(args.instrumented, fixture, after)
        except ValueError as error:
            validation_error = str(error)
            code = 1
    receipt = {'argv': command, 'wrapper_argv': sys.argv, 'mode': mode, 'instrumented': args.instrumented,
               'selection_mode': 'exact-name' if args.select_device_name is not None else
                                 'dummy-first-enumerated' if args.select_first_enumerated else 'default',
               'selected_source_only': args.selected_source_only, 'copied_selections': selections,
               'abi_profile': 1, 'injected_transport_failure': args.inject_put_failure, 'exit': code,
               'seconds': elapsed, 'SDL_AUDIO_DRIVER': env.get('SDL_AUDIO_DRIVER'),
               'files_before': before, 'files_after': after, 'unchanged': before == after,
               'source_files_before': source_before, 'source_files_after': source_after,
               'source_unchanged': source_before == source_after, 'observed': observed,
               'validation_error': validation_error, 'physical_audibility': 'UNQUALIFIED',
               'physical_removal': 'UNRUN',
               'music_evidence': 'UNOPENED' if args.selected_source_only else
                                 'successful PCM submission progress' if args.instrumented else 'playback state only',
               'public_guid_selected_api': 'PASS' if observed['public_guid_selected_api'] else
                                           'FAIL' if selected else 'NOT_REQUESTED'}
    (args.evidence / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(args.evidence, code)
    return code


def self_test():
    """Pure admission/receipt tests: no module load, subprocess, or device actions."""
    import contextlib
    import io
    import unittest

    class AdmissionTests(unittest.TestCase):
        def parse(self, extra, env=None):
            return parse_options(['--graph', '/graph', '--fixture', '/fixture', '--profile', 'Release',
                                  '--evidence', '/evidence', *extra], {} if env is None else env)

        def test_dummy_and_exact_utf8_selection(self):
            self.assertEqual(self.parse(['--select-device-name', 'Full ł label']).select_device_name, 'Full ł label')
            self.assertTrue(self.parse(['--select-first-enumerated', '--selected-source-only']).selected_source_only)
            self.assertFalse(self.parse([]).root_authorized_live_default)

        def test_wrong_live_and_selection_combinations(self):
            for extra in [
                ['--root-authorized-live-selected'],
                ['--root-authorized-live-default', '--select-device-name', 'x'],
                ['--root-authorized-live-selected', '--select-first-enumerated'],
                ['--select-first-enumerated', '--select-device-name', 'x'],
                ['--selected-source-only'], ['--inject-put-failure'],
                ['--root-authorized-live-selected', '--select-device-name', 'x'],
                ['--root-authorized-live-default', '--root-authorized-live-selected'],
            ]:
                with self.subTest(extra=extra), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                    self.parse(extra)
            # Parsing only: the authorization test never dispatches a live process.
            live = ['--root-authorized-live-selected', '--select-device-name', 'complete name']
            self.assertEqual(self.parse(live, {'CSE_ROOT_LIVE_OUTPUT_AUTHORIZED': '1'}).select_device_name,
                             'complete name')
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                self.parse(live, {'CSE_ROOT_LIVE_OUTPUT_AUTHORIZED': '1', 'SDL_AUDIO_DRIVER': 'dummy'})

        def test_instrumented_capture_hash_is_required(self):
            fixture = Path('/fixture')
            with self.assertRaises(ValueError):
                validate_capture_binding(True, fixture, {})
            with self.assertRaises(ValueError):
                validate_capture_binding(True, fixture, {'/fixture/liba3d_live_capture.dylib': ''})
            validate_capture_binding(True, fixture, {'/fixture/liba3d_live_capture.dylib': 'a' * 64})
            validate_capture_binding(False, fixture, {})

        def test_music_and_selection_receipts_fail_closed(self):
            mode = {'instrumented': True, 'selected_source_only': False, 'selected': True}
            complete = {'completed': True, 'music_pcm_progress': True,
                        'public_guid_selected_api': True, 'explicit_selected_open': True}
            validate_observation(mode, complete)
            for field in complete:
                with self.subTest(field=field), self.assertRaises(ValueError):
                    validate_observation(mode, {**complete, field: False})
            source_only = {**mode, 'selected_source_only': True}
            with self.assertRaises(ValueError):
                validate_observation(source_only, complete)
            validate_observation(source_only, {**complete, 'music_pcm_progress': False, 'no_default_opens': True})
            validate_observation({'instrumented': False, 'selected_source_only': False}, {'completed': True})

    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(AdmissionTests))
    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    sys.exit(self_test() if sys.argv[1:] == ['--self-test'] else main())
