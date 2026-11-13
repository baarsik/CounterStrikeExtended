#!/usr/bin/env python3
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
out=root/'build/live-localization/cs-tests'
out.mkdir(parents=True,exist_ok=True)
subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-self-assign-overloaded','-Wno-self-move',
                '-fsanitize=address,undefined','-I',str(root/'src/public'),
                str(root/'tests/native/live_localization_cs/snapshot_tests.cpp'),'-o',str(out/'snapshot')],check=True)
subprocess.run([str(out/'snapshot')],check=True)
for suite in ['owner_tests.py','text_tests.py','hud_tests.py','center_tests.py','chat_tests.py','menu_tree_tests.py']:
    subprocess.run(['python3',str(root/'tests/native/live_localization_cs'/suite)],check=True)
print('CS live-localization: snapshot and six production-owner suites passed (ASan/UBSan).')
