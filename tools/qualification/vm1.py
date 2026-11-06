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
from pathlib import Path


def build_command(script, *, vm="Windows 11", system=False):
    command = ["prlctl", "exec", vm]
    if not system:
        command.append("--current-user")
    command.extend(["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-EncodedCommand",
                    base64.b64encode(script.encode("utf-16-le")).decode("ascii")])
    return command


def interactive_script(script, timeout):
    encoded = base64.b64encode(script.encode("utf-16-le")).decode("ascii")
    if len(encoded) > 30000:
        raise ValueError("interactive script is too large; pass a short wrapper invoking a guest .ps1 file")
    return r"""
$ErrorActionPreference = 'Stop'
$user = (Get-CimInstance Win32_ComputerSystem).UserName
if (-not $user) { throw 'No logged-on Windows desktop user' }
$name = 'CSE-Probe-' + [Guid]::NewGuid().ToString('N')
$action = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument '-NoProfile -ExecutionPolicy Bypass -EncodedCommand ENCODED_SCRIPT'
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
""".replace("ENCODED_SCRIPT", encoded).replace("TIMEOUT_SECONDS", str(timeout))


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
    if args.interactive:
        script = interactive_script(script, args.timeout)
    try:
        result = subprocess.run(build_command(script, vm=args.vm, system=args.system),
                                capture_output=True, text=True, timeout=args.timeout + 10)
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"VM transport failed: {error}", file=sys.stderr)
        return 1
    print(result.stdout, end="")
    print(result.stderr, end="", file=sys.stderr)
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
