"""Actual cold cohort/provider/root admission; stop before game/Steam initialization."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

probe, runtime = map(lambda name: Path(name).resolve(), sys.argv[1:])
with tempfile.TemporaryDirectory(prefix='cse actual startup ') as temporary:
    root = Path(temporary).resolve()
    app = root / 'Runtime β With Spaces'
    shutil.copytree(runtime, app)
    shutil.copy2(probe, app / 'hl')
    (app / 'root-marker.txt').write_text('owned-root-marker')
    caller = root / 'unrelated caller'
    caller.mkdir()
    caller_settings = 'EngineDLL=hw.dll\nScreenWindowed=0\nDisplayWidth=320\nDisplayHeight=240\n'
    (caller / 'hl.conf').write_text(caller_settings)
    owned_settings = 'EngineDLL=sw.dll\nScreenWindowed=1\nDisplayWidth=800\nDisplayHeight=600\n'
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(('DYLD_', 'CSE_BOOTSTRAP_'))}
    env.update(SDL_VIDEO_DRIVER='dummy', SDL_RENDER_DRIVER='software')

    def launch(case, cwd, executable=None, arguments=(), error=None, expected_error=None):
        (app / 'hl.conf').write_text(owned_settings)
        case_env = dict(env)
        if error:
            case_env['CSE_BOOTSTRAP_PATH_ERROR'] = error
        result = subprocess.run([str(executable or app / 'hl'), *arguments], cwd=cwd, env=case_env,
                                capture_output=True, text=True, timeout=20)
        report = dict(case=case, exit=result.returncode, stdout=result.stdout, stderr=result.stderr)
        print(json.dumps(report, ensure_ascii=False), flush=True)
        assert (caller / 'hl.conf').read_text() == caller_settings, report
        if expected_error:
            assert result.returncode == 1 and expected_error in result.stderr, report
            assert 'reached_run=1' not in result.stdout, report
            if error:
                assert (app / 'hl.conf').read_text() == owned_settings, report
            return
        assert result.returncode == 0, report
        for line in (f'cwd={app}', f'base={app}', f'engine={app}/swds.dylib',
                     f'provider={app}/libsteam_api.dylib', 'actual_filesystem_root=read',
                     'windowed=1', 'width=800', 'height=600', 'reached_run=1'):
            assert line in result.stdout.splitlines(), (line, report)
        if arguments:
            assert '-game cstrike' in result.stdout, report
        return report

    # Removing/late-moving the launcher cwd initialization breaks these real
    # provider binds and chooses the unrelated caller's renderer/settings.
    launch('no-arguments-root', '/')
    launch('no-arguments-unrelated', caller)
    alias = caller / 'cstrike'
    alias.symlink_to(app / 'hl')
    report = launch('canonical-symlink-alias', '/', alias)
    assert '-game cstrike' in report['stdout'], report
    (caller / 'options.txt').write_text('-game cstrike')
    launch('caller-relative-response-file', caller, arguments=('@options.txt',))
    nested = root / ('nested ' + 'a' * 90) / ('owned ' + 'b' * 90) / ('leaf ' + 'c' * 70)
    nested.parent.mkdir(parents=True)
    app.rename(nested)
    app = nested
    assert len(os.fsencode(app)) > 260
    launch('long-canonical-runtime-path', '/')
    for error, diagnostic in (
        ('executable-too-long', 'Executable path exceeds'),
        ('unresolvable-executable', 'Failed to resolve the launcher executable path'),
        ('missing-directory', 'Unable to enter the native runtime directory'),
        ('denied-directory', 'Unable to enter the native runtime directory'),
    ):
        launch(error, '/', error=error, expected_error=diagnostic)
    provider = app / 'libsteam_api.dylib'
    hidden = app / 'provider-disabled'
    provider.rename(hidden)
    launch('missing-real-provider', '/', expected_error='Unable to bind native Steam provider')
    # A real incompatible library cannot satisfy the Steam export-table contract.
    shutil.copy2(app / 'filesystem_stdio.dylib', provider)
    launch('incompatible-provider', '/', expected_error='SteamAPI_Init')
    provider.unlink()
    engine = app / 'swds.dylib'
    original = hashlib.sha256(engine.read_bytes()).hexdigest()
    with engine.open('ab') as image:
        image.write(b'cohort mismatch')
    launch('modified-software-before-constructors', '/', expected_error='do not match')
    # The provider is still missing: if an engine constructor had run before
    # cohort admission, it would instead have failed with the Steam diagnostic.
    hidden.rename(provider)
    print(json.dumps({'actual_software_sha256_before_rejection': original,
                      'actual_provider_sha256': hashlib.sha256(provider.read_bytes()).hexdigest()}))
print('PASS actual startup cwd/provider/cohort/registry and pre-Run shutdown; no SteamAPI_Init')
