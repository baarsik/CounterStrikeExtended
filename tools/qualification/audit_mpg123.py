#!/usr/bin/env python3
"""Audit the music provider on top of the existing formal Windows import closure check."""
import argparse
import json
from pathlib import Path
import sys

from audit_sdl3 import audit_runtime

MILES_MODULES = {'mss32.dll', 'mss64.dll', 'mssmp3.asi', 'mssvoice.asi', 'binkawin.asi'}


def require_no_miles(root, report):
    for path in root.rglob('*'):
        if path.is_file() and path.name.lower() in MILES_MODULES:
            raise ValueError('Retired Miles provider in runtime: ' + str(path.relative_to(root)))
    binaries = report['binaries']
    for name, info in binaries.items():
        for imported in info['imports']:
            if imported.lower() in MILES_MODULES:
                raise ValueError(f'Miles import remains: {name} -> {imported}')
    by_name = {name.lower(): info for name, info in binaries.items()}
    for engine in ['hw.dll', 'swds.dll']:
        if engine not in by_name or 'mpg123.dll' not in {name.lower() for name in by_name[engine]['imports']}:
            raise ValueError(engine + ' must import the source-built mpg123.dll')
    if 'mpg123.dll' not in by_name:
        raise ValueError('Missing source-built mpg123.dll')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runtime', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    try:
        report = audit_runtime(args.runtime)
        require_no_miles(args.runtime, report)
        args.output.write_text(json.dumps(report, indent=2) + '\n')
    except (OSError, ValueError) as error:
        print('Music provider audit failed: ' + str(error), file=sys.stderr)
        return 1
    print('Music provider/import closure passed: ' + str(args.output))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
