from pathlib import Path
import json,subprocess,sys
executable=Path(sys.argv[1])
rows=[]
for scenario,phrase in [('fatal-clock','cold clock'),('fatal-unbound','not bound'),('fatal-worker','owner thread'),('fatal-closing','closing')]:
 result=subprocess.run([str(executable),scenario],capture_output=True,text=True,timeout=20)
 assert result.returncode!=0 and phrase in result.stderr,(scenario,result.returncode,result.stderr)
 rows.append({'scenario':scenario,'exit':result.returncode,'diagnostic':phrase})
print(json.dumps(rows))
