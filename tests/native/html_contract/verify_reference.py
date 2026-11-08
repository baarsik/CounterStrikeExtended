"""Compare the retained Windows virtual sequence with the canonical shared declarations."""
import argparse
from pathlib import Path
import re


def declarations(text, name):
    body=text[text.index('class '+name):]
    body=body[:body.index('\n};')]
    # Whitespace and pure markers do not change a member's declared signature.
    return [re.sub(r'\s+','',s.replace('= 0','')) for s in re.findall(r'virtual\s+[^;]+;',body)]


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('reference',type=Path)
    parser.add_argument('canonical',type=Path)
    args=parser.parse_args()
    old=args.reference.read_bytes().decode('cp1252')
    current=args.canonical.read_text()
    for name,expected in [('IHTMLResponses_HL1',25),('IHTMLSerializer',28)]:
        before=declarations(old,name)
        after=declarations(current,name)
        assert len(before)==expected and before==after,(name,before,after)
    print('Retained Windows HTML virtual declarations match: responses25, serializer28')
