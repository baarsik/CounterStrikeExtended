#!/usr/bin/env python3
"""Execute a local PowerShell file in an existing Parallels VM without changing its configuration.

Use --interactive for a short wrapper invoking invoke_game_probe.ps1 through an
already configured guest/shared path. The task uses the logged-on Windows user;
--system selects the bootstrap transport account, not the game's desktop account.
"""

import argparse
import base64
import subprocess
import sys
import time
import uuid
from pathlib import Path

MAX_ENCODED_ARGUMENT = 2800  # Parallels transport fails around 4 KiB on the qualified VM.
CHUNK_BYTES = 384


def build_command(script, *, vm="Windows 11", system=False):
    command = ["prlctl", "exec", vm]
    if not system:
        command.append("--current-user")
    command.extend(["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-EncodedCommand",
                    base64.b64encode(script.encode("utf-16-le")).decode("ascii")])
    return command


def _guest_path(filename):
    return "[IO.Path]::Combine([IO.Path]::GetTempPath(), '" + filename + "')"


def _staging_commands(script, filename):
    # Windows PowerShell 5 needs the BOM to recognize non-ASCII UTF-8 source.
    payload = script.encode("utf-8-sig")
    for offset in range(0, len(payload), CHUNK_BYTES):
        chunk = base64.b64encode(payload[offset:offset + CHUNK_BYTES]).decode("ascii")
        prefix = "$ErrorActionPreference='Stop';$p=" + _guest_path(filename)
        prefix += ";$b=[Convert]::FromBase64String('" + chunk + "');"
        if offset == 0:
            yield prefix + "$f=[IO.File]::Open($p,'CreateNew','Write');try{$f.Write($b,0,$b.Length)}finally{$f.Dispose()}"
        else:
            yield prefix + "$f=[IO.File]::Open($p,'Append','Write');try{$f.Write($b,0,$b.Length)}finally{$f.Dispose()}"


def interactive_script(filename, timeout):
    return r"""
$ErrorActionPreference = 'Stop'
$user = (Get-CimInstance Win32_ComputerSystem).UserName
if (-not $user) { throw 'No logged-on Windows desktop user' }
$name = 'CSE-Probe-' + [Guid]::NewGuid().ToString('N')
$p = GUEST_SCRIPT_PATH
$arguments = '-NoProfile -ExecutionPolicy Bypass -File "' + $p + '"'
$action = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument $arguments
$principal = New-ScheduledTaskPrincipal -UserId $user -LogonType Interactive -RunLevel Highest
$settings = New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Seconds TIMEOUT_SECONDS)
try {
    Register-ScheduledTask -TaskName $name -Action $action -Principal $principal -Settings $settings | Out-Null
    $started = Get-Date
    Start-ScheduledTask -TaskName $name
    $deadline = $started.AddSeconds(TIMEOUT_SECONDS)
    do {
        Start-Sleep -Milliseconds 250
        $task = Get-ScheduledTask -TaskName $name
        $info = Get-ScheduledTaskInfo -TaskName $name
        if ($info.LastRunTime -ge $started.AddSeconds(-1) -and $task.State -ne 'Running') {
            if ($info.LastTaskResult -ne 0) { throw "Interactive task failed: $($info.LastTaskResult)" }
            Write-Output 'INTERACTIVE_TASK_COMPLETED'
            exit 0
        }
    } while ((Get-Date) -lt $deadline)
    throw 'Interactive task timed out; inspect probe evidence and owned-PID cleanup'
} finally {
    $task = Get-ScheduledTask -TaskName $name -ErrorAction SilentlyContinue
    if ($task) {
        if ($task.State -eq 'Running') { Stop-ScheduledTask -TaskName $name }
        Unregister-ScheduledTask -TaskName $name -Confirm:$false
    }
}
""".replace("GUEST_SCRIPT_PATH", _guest_path(filename)).replace("TIMEOUT_SECONDS", str(timeout))


class _StageFailure(Exception):
    def __init__(self, result):
        self.result = result


def run_script(script, *, vm="Windows 11", system=False, timeout=180, interactive=False, runner=subprocess.run):
    deadline = time.monotonic() + timeout
    owned = []
    result = None

    def invoke(text):
        command = build_command(text, vm=vm, system=system)
        if len(command[-1]) > MAX_ENCODED_ARGUMENT:
            raise ValueError("internal transport command exceeds the encoded argument bound")
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise subprocess.TimeoutExpired(command, timeout)
        return runner(command, capture_output=True, text=True, timeout=remaining)

    def stage(text):
        filename = "CSE-vm1-" + uuid.uuid4().hex + ".ps1"
        owned.append(filename)
        for command in _staging_commands(text, filename):
            staged = invoke(command)
            if staged.returncode:
                raise _StageFailure(staged)
        return filename

    try:
        if interactive or len(build_command(script, vm=vm, system=system)[-1]) > MAX_ENCODED_ARGUMENT:
            filename = stage(script)
            if interactive:
                remaining = max(1, int(deadline - time.monotonic()))
                filename = stage(interactive_script(filename, remaining))
            command = "$p=" + _guest_path(filename)
            command += ";& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $p;exit $LASTEXITCODE"
            result = invoke(command)
        else:
            result = invoke(script)
    except _StageFailure as error:
        result = error.result
    finally:
        if owned:
            paths = ','.join('(' + _guest_path(filename) + ')' for filename in owned)
            cleanup = "$ErrorActionPreference='Stop';foreach($p in @(" + paths + ")){"
            cleanup += "if(Test-Path -LiteralPath $p){Remove-Item -LiteralPath $p -Force}}"
            try:
                cleaned = runner(build_command(cleanup, vm=vm, system=system),
                                 capture_output=True, text=True, timeout=5)
                if cleaned.returncode:
                    print("Owned guest temp-file cleanup failed: " + cleaned.stderr, file=sys.stderr)
                    if result is not None and result.returncode == 0:
                        result = cleaned
            except (OSError, subprocess.TimeoutExpired) as error:
                if result is not None and result.returncode == 0:
                    raise
                print(f"Owned guest temp-file cleanup failed: {error}", file=sys.stderr)
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("script", nargs="?", type=Path, help="local PowerShell file; stdin when omitted")
    parser.add_argument("--vm", default="Windows 11")
    parser.add_argument("--system", action="store_true")
    parser.add_argument("--interactive", action="store_true")
    parser.add_argument("--timeout", type=int, default=180, help="bounded transport/task timeout in seconds")
    args = parser.parse_args(argv)
    if not 10 <= args.timeout <= 600:
        parser.error("--timeout must be between 10 and 600 seconds")
    script = args.script.read_text(encoding="utf-8-sig") if args.script else sys.stdin.read()
    try:
        result = run_script(script, vm=args.vm, system=args.system, timeout=args.timeout,
                            interactive=args.interactive)
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"VM transport failed: {error}", file=sys.stderr)
        return 1
    print(result.stdout, end="")
    print(result.stderr, end="", file=sys.stderr)
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
