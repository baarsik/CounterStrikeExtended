#!/usr/bin/env python3
"""Emit inputs, copy a small fixture ZIP to the authorized Parallels test area and run x86 GDI checks."""
import argparse
import base64
import csv
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

HERE = Path(__file__).resolve().parent
BEGIN = 'CSE_FONT_REPORT_BEGIN'
END = 'CSE_FONT_REPORT_END'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--vm', default='Windows 11')
    parser.add_argument('--prlctl', default='/usr/local/bin/prlctl')
    parser.add_argument('--vm-directory', default=r'C:\CI\CSE-BundledTranslations-20261113\windows-fit-v1')
    parser.add_argument('--vcvars', default=r'C:\BuildTools\2026\VC\Auxiliary\Build\vcvarsall.bat')
    args = parser.parse_args()
    # This runner never changes VM configuration and confines writes to its dedicated test folder.
    if not args.vm_directory.startswith('C:\\CI\\CSE-BundledTranslations-20261113\\windows-fit-'):
        parser.error('--vm-directory must be a windows-fit-* folder inside the authorized test area')
    args.output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='cse-windows-font-') as temporary:
        temporary = Path(temporary)
        input_path = temporary / 'input.tsv'
        subprocess.run([sys.executable, str(HERE / 'emit_inputs.py'), '--game-root', str(args.game_root),
                        '--output', str(input_path)], check=True)
        archive = io.BytesIO()
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as package:
            package.write(HERE / 'font_fit.cpp', 'font_fit.cpp')
            package.write(input_path, 'input.tsv')
            package.write(input_path.with_suffix('.provenance.json'), 'input.provenance.json')
        script = r'''
$ErrorActionPreference = 'Stop'
$directory = __DIRECTORY__
[System.IO.Directory]::CreateDirectory($directory) | Out-Null
$zip = Join-Path $directory 'fixture.zip'
[System.IO.File]::WriteAllBytes($zip, [Convert]::FromBase64String([Console]::In.ReadToEnd()))
Expand-Archive -LiteralPath $zip -DestinationPath $directory -Force
Set-Location -LiteralPath $directory
$build = 'call "__VCVARS__" x86 >nul && cl /nologo /EHsc /std:c++17 /W4 /utf-8 font_fit.cpp /Fe:font_fit.exe /link gdi32.lib'
& $env:ComSpec /d /s /c $build
if ($LASTEXITCODE -ne 0) { throw 'x86 fixture compile failed' }
& .\font_fit.exe input.tsv fit.tsv
$fitExit = $LASTEXITCODE
Write-Output ('FIXTURE_EXIT=' + $fitExit)
Write-Output 'CSE_FONT_REPORT_BEGIN'
Write-Output ([Convert]::ToBase64String([System.IO.File]::ReadAllBytes((Join-Path $directory 'fit.tsv'))))
Write-Output 'CSE_FONT_REPORT_END'
'''
        script = script.replace('__DIRECTORY__', "'" + args.vm_directory.replace("'", "''") + "'")
        script = script.replace('__VCVARS__', args.vcvars)
        encoded = base64.b64encode(script.encode('utf-16-le')).decode('ascii')
        command = [args.prlctl, 'exec', args.vm, 'powershell.exe', '-NoProfile', '-EncodedCommand', encoded]
        result = subprocess.run(command, input=base64.b64encode(archive.getvalue()).decode('ascii'),
                                text=True, capture_output=True, timeout=180)
        if BEGIN not in result.stdout or END not in result.stdout:
            print(result.stdout[-5000:])
            print(result.stderr[-1000:])
            raise RuntimeError('VM did not return a font measurement report')
        prefix, body = result.stdout.split(BEGIN, 1)
        payload, suffix = body.split(END, 1)
        report = base64.b64decode(payload.strip())
        (args.output / 'fit.tsv').write_bytes(report)
        (args.output / 'input.provenance.json').write_bytes(input_path.with_suffix('.provenance.json').read_bytes())
        rows = list(csv.DictReader(io.StringIO(report.decode('utf-8-sig')), delimiter='\t'))
        failures = [row for row in rows if row['result'] in ('overflow', 'missing-glyph')]
        (args.output / 'overflows.json').write_text(json.dumps(failures, ensure_ascii=False, indent=2) + '\n')
        summary = {'vm': args.vm, 'architecture': 'x86 (compile-time assertion)', 'control_counts': {},
                   'alphabet_passes': sum(row['result'] == 'glyphs-pass' for row in rows),
                   'negative_controls_detected': sum(row['result'] == 'expected-overflow' for row in rows),
                   'failures': len(failures), 'report': str(args.output / 'fit.tsv')}
        for language in ('russian', 'polish'):
            selected = [row for row in rows if row['language'] == language and row['source'] not in
                        ('catalogue-alphabet', 'negative-control')]
            summary['control_counts'][language] = len(selected)
        (args.output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
        print(prefix.strip())
        print(json.dumps(summary, indent=2))
        return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
