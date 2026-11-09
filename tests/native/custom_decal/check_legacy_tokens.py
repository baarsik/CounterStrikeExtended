from pathlib import Path
import argparse,hashlib,json,re
parser=argparse.ArgumentParser();parser.add_argument('--repo',type=Path,required=True);parser.add_argument('--reference',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
paths=['engine/com_custom.cpp','engine/decals.cpp','engine/draw.h','engine/draw.cpp','engine/common.cpp']
start=re.compile(r'^\s*#\s*(if|ifdef|ifndef)\b')
branch=re.compile(r'^\s*#\s*(else|elif)\b')
finish=re.compile(r'^\s*#\s*endif\b')
def fold(lines,windows_guard=False):
 out=[];index=0
 while index<len(lines):
  line=lines[index]
  if not start.match(line):out.append(line);index+=1;continue
  normalized=re.sub(r'\s+','',line)
  decision=None
  if normalized in ['#ifdefined(_WIN64)||defined(__LP64__)','#ifINTPTR_MAX>INT32_MAX']:decision=False
  if windows_guard and normalized=='#ifdefined(_WIN32)':decision=True
  depth=1;end=index+1;boundaries=[]
  while end<len(lines):
   if start.match(lines[end]):depth+=1
   elif finish.match(lines[end]):
    depth-=1
    if depth==0:break
   elif depth==1 and branch.match(lines[end]):boundaries.append(end)
   end+=1
  assert depth==0
  points=[index]+boundaries+[end]
  if decision is not None:
   assert len(boundaries)<=1 and (not boundaries or re.match(r'^\s*#\s*else\b',lines[boundaries[0]]))
   if decision:out+=fold(lines[index+1:points[1]],windows_guard)
   elif boundaries:out+=fold(lines[boundaries[0]+1:end],windows_guard)
  else:
   for first,last in zip(points,points[1:]):out.append(lines[first]);out+=fold(lines[first+1:last],windows_guard)
   out.append(lines[end])
  index=end+1
 return out
pattern=re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*.*?\*/|[A-Za-z_]\w*|\d+|[^\s]',re.S)
def tokens(text):return [x for x in pattern.findall(text) if not x.startswith(('//','/*'))]
rows=[]
for relative in paths:
 before=(args.reference/relative).read_text();after=(args.repo/'GoldSrc-SDK'/relative).read_text()
 a=tokens(''.join(fold(before.splitlines(True),relative=='engine/com_custom.cpp')));b=tokens(''.join(fold(after.splitlines(True),relative=='engine/com_custom.cpp')))
 if a!=b:
  i=next((i for i,(x,y) in enumerate(zip(a,b)) if x!=y),min(len(a),len(b)));print(relative,'DIFF',i,a[max(i-8,0):i+15],b[max(i-8,0):i+15]);raise SystemExit(17)
 rows.append(dict(relative=relative,tokens=len(a),sha256=hashlib.sha256('\0'.join(a).encode()).hexdigest(),equal=True))
args.output.write_text(json.dumps(rows,indent=2)+'\n');print('Five Win32 selected-token controls match frozen reference')
