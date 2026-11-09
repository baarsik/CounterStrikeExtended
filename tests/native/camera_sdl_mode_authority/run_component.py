"""Bounded source/body gate. Never links or executes an SDL provider."""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import os
import struct
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--repo', required=True)
parser.add_argument('--sdl-source', required=True)
parser.add_argument('--build', required=True)
parser.add_argument('--config', choices=['Release','Debug'], required=True)
parser.add_argument('--evidence', required=True)
parser.add_argument('--sanitizers', action='store_true')
args = parser.parse_args()
repo=Path(args.repo).resolve()
build=Path(args.build).resolve()
out=Path(args.evidence).resolve()
out.mkdir(parents=True,exist_ok=False)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
receipt=dict(config=args.config,sanitizers=args.sanitizers,game_or_provider_executed=False,
             production_readiness=False,commands=[])

def run(name,command,expected=0):
    with (out/(name+'.log')).open('wb') as log:
        result=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=120)
    receipt['commands'].append(dict(name=name,argv=command,exit=result.returncode,expected=expected))
    if result.returncode != expected:
        raise RuntimeError((name,result.returncode,expected))

try:
    command=['cmake','-S',str(repo/'tests/native/camera_sdl_mode_authority'),'-B',str(build),'-G','Ninja',
             '-DCMAKE_BUILD_TYPE='+args.config,'-DCSE_SDL_SOURCE_DIR='+str(Path(args.sdl_source).resolve()),
             '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DCSE_MODE_SANITIZERS='+('ON' if args.sanitizers else 'OFF')]
    if __import__('sys').platform == 'darwin':
        command.append('-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0')
    run('configure',command)
    run('build',['cmake','--build',str(build),'--parallel','2'])
    images=[]
    for game in ['cs','hl']:
        image=build/('mode_'+game+('.exe' if os.name=='nt' else ''))
        run(game,[str(image)])
        run(game+'-naive-authority-red',[str(image),'--naive-authority-hypothesis'],17)
        record=dict(path=str(image),bytes=image.stat().st_size,sha256=sha(image))
        if os.name=='nt':
            data=image.read_bytes();pe=struct.unpack_from('<I',data,60)[0]
            record['machine']=struct.unpack_from('<H',data,pe+4)[0]
        images.append(record)
    receipt['images']=images
    receipt['compile_commands']=json.loads((build/'compile_commands.json').read_text())
    receipt['imported_sdk']=json.loads((build/'imported/imported-sdl.json').read_text())
    receipt['camera_bodies']=json.loads((build/'imported/extracted.json').read_text())
    receipt['caller_blocks']=json.loads((build/'imported/caller-blocks.json').read_text())
    receipt['success']=True
except BaseException as error:
    receipt.update(success=False,failure=repr(error))
finally:
    receipt['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat()
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
if not receipt['success']:
    raise SystemExit(1)
