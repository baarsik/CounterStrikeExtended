"""Compile each deliberate variant; require assertion exit17, never count compiler errors as RED."""
from pathlib import Path
import argparse
import json
import subprocess

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--repo',required=True)
parser.add_argument('--sdl-source',required=True)
parser.add_argument('--build-root',required=True)
parser.add_argument('--evidence',required=True)
a=parser.parse_args()
repo=Path(a.repo).resolve();build_root=Path(a.build_root).resolve();out=Path(a.evidence).resolve()
out.mkdir(parents=True,exist_ok=False)
records=[]
try:
    for name in ['no-prohibition','failed-global-commit','relative-cache-stale','warp-consumes-accumulator',
                 'nochange-updates-global','no-flag-rollback','caller-failure-commit']:
        build=build_root/name
        commands=[('configure',['cmake','-S',str(repo/'tests/native/camera_sdl_mode_authority'),'-B',str(build),
            '-G','Ninja','-DCMAKE_BUILD_TYPE=Debug','-DCSE_SDL_SOURCE_DIR='+str(Path(a.sdl_source).resolve()),
            '-DCSE_MODE_MUTANT='+name],0),
            ('build',['cmake','--build',str(build),'--parallel','2'],0),
            ('run',[str(build/('mode_cs.exe' if __import__('os').name=='nt' else 'mode_cs'))],17)]
        if __import__('sys').platform == 'darwin':
            commands[0][1].append('-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0')
        for phase,command,expected in commands:
            with (out/(name+'-'+phase+'.log')).open('wb') as log:
                r=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=120)
            records.append(dict(name=name,phase=phase,argv=command,exit=r.returncode,expected=expected))
            assert r.returncode==expected,(name,phase,r.returncode,expected)
finally:
    (out/'receipt.json').write_text(json.dumps(dict(records=records,provider_executed=False,
        production_source_changed=False),indent=2)+'\n')
