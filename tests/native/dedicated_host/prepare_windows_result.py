"""Compile Main's actual terminal return expression, not a copied mapping."""
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parents[3]
source = (root / 'src/dedicated/sys_ded.cpp').read_text()
main = source[source.index('int Main('):source.index('\nstatic void LaunchSteam(')]
expression = re.findall(r'return ([^;]+);', main)[-1]
includes = '\n'.join(line for line in source.splitlines() if 'DedicatedResult.h"' in line)
# sys_ded's relative include is resolved by the owning dedicated include directory.
output = pathlib.Path(sys.argv[1])
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text('#include <initializer_list>\n' + includes + '\nint ActualMainResult(int result) {\n'
                  + '    return ' + expression + ';\n}\n#include "windows_result.cpp"\n')
