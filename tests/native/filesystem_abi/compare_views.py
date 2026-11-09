"""Fail when independently compiled actual header views disagree on the public prefix."""

import json
import subprocess
import sys


views = {name: json.loads(subprocess.check_output([path], text=True))
         for name, path in zip(("public", "launcher", "vgui", "provider"), sys.argv[1:])}
provider = views["provider"]
different = False
for name in ("public", "launcher", "vgui"):
    for consumer, offered in zip(views[name]["methods"], provider["methods"]):
        if consumer["slot"] != offered["slot"] or consumer["shape"] != offered["shape"]:
            print(f'{name} vs provider: ordinal={consumer["ordinal"]} slot={consumer["slot"]} '
                  f'{consumer["name"]}: consumer={consumer["shape"]} provider={offered["shape"]}')
            different = True
if different:
    print("filesystem_actual_header_parity=FAILED")
    sys.exit(1)
print("filesystem_actual_header_parity=passed")
