"""Project a retained GameDLL definition onto a native Windows compiler ABI.

The source EXPORT declarations emit native C++ names. Explicit x86 decorated
names cannot be linked by x64/ARM64; C entry points and their ordinals stay fixed.
"""
import re


def native_definition(source):
    result = []
    library = False
    exports = False
    sections = False
    for line in source.splitlines():
        text = line.split(';', 1)[0].strip()
        if not text:
            result.append(line)
            continue
        if re.fullmatch(r'LIBRARY\s+[A-Za-z_][\w.-]*', text, re.IGNORECASE) and not library and not exports:
            library = True
        elif text.upper() == 'EXPORTS' and library and not exports:
            exports = True
        elif text.upper() == 'SECTIONS' and exports and not sections:
            sections = True
        elif sections and re.fullmatch(r'\.[\w]+(?:\s+(?:READ|WRITE|EXECUTE|SHARED))+', text, re.IGNORECASE):
            pass
        elif exports and not sections:
            if text.upper() in ('LIBRARY', 'NAME', 'DESCRIPTION', 'EXPORTS', 'IMPORTS', 'SECTIONS',
                                'STACKSIZE', 'HEAPSIZE', 'VERSION'):
                raise ValueError('Unsupported GameDLL definition directive: ' + text)
            alias = re.fullmatch(r'([A-Za-z_]\w*)\s*=\s*_([A-Za-z_]\w*)@\d+(\s+@\d+)?', text)
            if alias and alias[1] == alias[2]:
                # Native Windows uses the undecorated C name, with the existing ordinal.
                indentation = line[:len(line) - len(line.lstrip())]
                result.append(indentation + alias[1] + (alias[3] or ''))
                continue
            if re.fullmatch(r'\?\S*@@\S*(?:\s+@\d+)?', text):
                continue
            if not re.fullmatch(r'[A-Za-z_]\w*(?:\s+@\d+)?(?:\s+(?:NONAME|DATA|PRIVATE))*', text):
                raise ValueError('Unsupported GameDLL export: ' + text)
        else:
            raise ValueError('Unsupported GameDLL definition line: ' + text)
        result.append(line)
    if not library or not exports:
        raise ValueError('GameDLL definition requires LIBRARY and EXPORTS')
    return '\n'.join(result) + '\n'
