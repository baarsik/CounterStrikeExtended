import json,sys
from pathlib import Path
reference=[json.loads(s) for s in (Path(__file__).with_name("win32_reference.jsonl")).read_text().splitlines()]
actual=[json.loads(s) for s in Path(sys.argv[1]).read_text().splitlines()]
assert len(reference)==len(actual)==748
assert actual[0]["kind"]=="facts"
assert actual[0]["app_deadline_bytes"]==actual[0]["edit_deadline_bytes"]==4
assert actual[0]["text_deadline_bytes"]==actual[0]["long_bytes"]
assert actual[0]["text_signed"]==1
errors=[(index,a,b) for index,(a,b) in enumerate(zip(actual[1:],reference[1:]),1) if a!=b]
assert not errors,(len(errors),errors[:2])
print("PASS747 actual-method rows match pinned twelve-profile Windows reference")
