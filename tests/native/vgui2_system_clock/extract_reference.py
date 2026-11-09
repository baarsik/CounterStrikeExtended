#!/usr/bin/env python3
"""Import full actual private CSystem declaration and selected arithmetic bodies."""
import argparse
import hashlib
import json
from pathlib import Path
import re

def block(text,start):
    brace=text.index('{',start);depth=1;end=brace+1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
source=args.source.resolve();text=source.read_bytes().decode('latin1').replace('\r\n','\n');out=args.output;out.mkdir(parents=True,exist_ok=True)
declaration=block(text,text.index('class CSystem'))+';'
methods={name:block(text,text.index(signature)) for name,signature in [('constructor','CSystem::CSystem()'),('destructor','CSystem::~CSystem()'),('current','double CSystem::GetCurrentTime()'),('millis','long CSystem::GetTimeMillis()')]}
public=declaration[:declaration.index('private:')]
slots=re.findall(r'virtual\s+(.+?)\s+([A-Za-z_][A-Za-z_0-9]*)\s*\((.*?)\)\s+override;',public,re.S);assert len(slots)==33
stubs=[]
for result,name,params in slots:
    if name in ['GetCurrentTime','GetTimeMillis']:continue
    result=re.sub(r'\s+',' ',result).strip();params=re.sub(r'\s+',' ',params).strip()
    # Only unnamed parameter declarations are needed for definitions, but retaining
    # actual names/types is clearer; known unrelated methods never execute.
    names=[re.search(r'([A-Za-z_]\w*)\s*$',part.strip()).group(1) for part in params.split(',') if part.strip()]
    ignored=' '.join('(void)'+parameter+';' for parameter in names)
    stubs.append(result+' CSystem::'+name+'('+params+') { '+ignored+' UnexpectedSystemApi("'+name+'"); }')
(out/'actual_class.inc').write_text(declaration+'\n')
(out/'actual_arithmetic.inc').write_text('\n\n'.join(methods.values())+'\n')
(out/'unrelated_methods.inc').write_text('\n'.join(stubs)+'\n')
(out/'imports.json').write_text(json.dumps({'source':str(source),'sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
 'actual_class_sha256':hashlib.sha256(declaration.encode()).hexdigest(),'methods':{name:hashlib.sha256(value.encode()).hexdigest() for name,value in methods.items()},
 'unrelated_methods':'Typed actual declarations with fatal bindings; not runtime feature implementation','public_api_methods':33},indent=2)+'\n')
