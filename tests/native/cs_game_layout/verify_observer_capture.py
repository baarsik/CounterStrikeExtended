#!/usr/bin/env python3
"""Validate actual in-game owner geometry from the isolated preview's capture log."""
import argparse
from pathlib import Path
import re
parser=argparse.ArgumentParser()
parser.add_argument('log',type=Path)
parser.add_argument('width',type=int)
parser.add_argument('height',type=int)
parser.add_argument('--class-phases',default='classct,classctgign,classglass,classt,classtarctic')
parser.add_argument('--caption-phases',default='collapsed,inset,specglass')
parser.add_argument('--motd-phase',default='motd')
args=parser.parse_args()
text=args.log.read_text(errors='replace')
parts=re.split(r'CSE_LAYOUT_CAPTURE\s*\n(\w+)\s*\n',text)
phases={parts[i]:parts[i+1].split('Wrote HalfLife')[0] for i in range(1,len(parts),2)}
def owner(phase):
    assert phase in phases, f'missing capture: {phase}'
    match=re.search(r'LAYOUT_OWNER \S+ rect=(-?\d+),(-?\d+),(\d+),(\d+)',phases[phase])
    assert match, f'missing visible owner: {phase}'
    return tuple(map(int,match.groups()))
def children(phase):
    return [(name,kind,int(visible),tuple(map(int,(x,y,w,h)))) for name,kind,visible,x,y,w,h in
            re.findall(r'LAYOUT_CHILD depth=\d+ (\S+) (\S+) visible=(\d) mouse=\d rect=(-?\d+),(-?\d+),(\d+),(\d+)',phases[phase])]
def child(phase,name):
    found=[(v,r) for n,k,v,r in children(phase) if n==name]
    assert len(found)==1, f'{phase}: missing/duplicate {name}'
    return found[0]
for phase in filter(None,args.class_phases.split(',')):
    x,y,w,h=owner(phase)
    assert abs(x+w/2-args.width/2)<=1 and abs(y+h/2-args.height/2)<=1, f'{phase}: shell not centered'
    assert x>=0 and y>=0 and x+w<=args.width and y+h<=args.height, f'{phase}: shell outside screen'
    visible=[(n,k,r) for n,k,v,r in children(phase) if v and (k in ['Button','Label','ImagePanel'])]
    for name,kind,(left,top,wide,tall) in visible:
        assert left>x and top>y and left+wide<=x+w and top+tall<=y+h, f'{phase}: {name} outside content inset'
    assert any(n=='joinClass' for n,_,_ in visible), f'{phase}: missing title'
    assert any(n=='classimage' for n,_,_ in visible), f'{phase}: missing portrait'
    for name in ['imageBG','imageBorder']:
        assert not child(phase,name)[0], f'{phase}: legacy preview decoration still visible'
for phase,image in [('classct','urban'),('classctgign','gign'),('classglass','gign'),
                    ('classt','terror'),('classtarctic','arctic')]:
    if phase in phases:
        assert f'LAYOUT_IMAGE classimage value=gfx/vgui/{image}' in phases[phase], f'{phase}: wrong hover preview'
for phase in filter(None,args.caption_phases.split(',')):
    owner(phase)
    labels=[r for n,k,v,r in children(phase) if n=='playerlabel' and v]
    assert len(labels)==1, f'{phase}: missing/duplicate target caption'
    x,y,w,h=labels[0]
    assert y>=args.height*0.85 and y+h<=args.height and x>=0 and x+w<=args.width, f'{phase}: target caption not at bottom'
    assert re.search(r'LAYOUT_TEXT playerlabel value=\S',phases[phase]), f'{phase}: empty target caption'
if 'expanded' in phases:
    for phase in ['expanded','expandednext']:
        visible,(x,y,w,h)=child(phase,'BottomBar')
        assert visible and x>=0 and y>=0 and x+w<=args.width and y+h==args.height, f'{phase}: expanded rail clipped'
        assert not child(phase,'playerlabel')[0], f'{phase}: collapsed caption overlaps expanded rail'
    before=re.search(r'LAYOUT_TEXT playerlabel value=(.+)',phases['expanded']).group(1)
    after=re.search(r'LAYOUT_TEXT playerlabel value=(.+)',phases['expandednext']).group(1)
    assert before!=after, 'expanded next-player control did not change the observer target'
if 'inset' in phases:
    assert child('inset','insetBox')[0], 'picture-in-picture did not open'
    assert child('inset','TopBar')[1][0]>0, 'picture-in-picture top-bar offset was lost'
    assert child('inset','playerlabel')[1]==child('collapsed','playerlabel')[1], 'inset moved target caption'
if 'classoriginalfresh' in phases:
    names=['joinClass','urban','gsg9','sas','gign','autoselect_ct','classimage','infolabel','imageBG','imageBorder']
    for name in names:
        assert child('classoriginal',name)==child('classoriginalfresh',name), f'Original replay changed {name} geometry'
    for name in ['imageBG','imageBorder']:
        assert child('classoriginal',name)[0], f'Original lost {name}'
if args.motd_phase:
    phase=args.motd_phase;owner(phase)
    labels=[(n,v) for n,k,v,r in children(phase) if n.lower()=='servername']
    assert len(labels)==1, 'MOTD must retain one managed server title'
    assert not any(v for n,v in labels), 'MOTD legacy title duplicates the glass header'
    assert 'server name goes here' not in phases[phase].lower(), 'MOTD resource placeholder still present'
if 'motdoriginal' in phases:
    assert child('motdoriginal','serverName')[0], 'Original lost its server title'
    assert not child('motdglass','serverName')[0], 'MOTD duplicated its title after switching back'
if 'teamafterMOTD' in phases:
    assert 'LAYOUT_OWNER TeamMenu ' in phases['teamafterMOTD'], 'MOTD OK did not return to team selection'
print('In-game captures passed: fighter previews, observer bars/inset/target cycling, MOTD and Original replay')
