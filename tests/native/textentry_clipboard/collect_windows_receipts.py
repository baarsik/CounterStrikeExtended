"""Hash the actual compiler-reported Windows include closure, not the environment."""
import argparse
import hashlib
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--art", type=Path, required=True)
args = parser.parse_args()
records = []
closure = {}
for directory in ["windows", "whole-windows"]:
    for source in sorted((args.art / directory).glob("*-dependencies.json")):
        data = json.loads(source.read_text(encoding="utf-8-sig"))
        assert data["Version"] == "1.2", source
        paths = [data["Data"]["Source"]] + data["Data"]["Includes"]
        for name in paths:
            path = Path(name)
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            if name in closure:
                assert closure[name] == digest, "input drift: " + name
            closure[name] = digest
        records.append({"report": str(source), "actual_inputs": len(paths)})
assert len(records) == 84, len(records)
(args.art / "windows-include-closure.json").write_text(json.dumps({"reports": records, "inputs": closure}, indent=2) + "\n")
print("PASS actual Windows input closure", len(records), "reports", len(closure), "unique paths")
