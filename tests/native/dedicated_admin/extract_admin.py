"""Compile verbatim retained remote owner without unrelated GUI widget dependencies."""
import pathlib
import sys
sdk = pathlib.Path(sys.argv[1])
out = pathlib.Path(sys.argv[2])
h = (sdk / 'dedicated/AdminServer.h').read_text(errors='surrogateescape')
c = (sdk / 'dedicated/AdminServer.cpp').read_text(errors='surrogateescape')
text = '''#include "utlbuffer.h"
#include "utllinkedlist.h"
#include "game_server_data.h"
#include <cstdint>
#include <cstring>
#include <limits>
#include <algorithm>
#include <vector>
#include <memory>
'''
text += h[h.index('#define SERVERDATA_REQUESTVALUE'):h.index('namespace vgui2')].replace('#include "../public/game_server_data.h"', '')
text += h[h.index('class IServerDataResponse'):h.index('class CPlayerContextMenu')]
text += '\nextern IGameServerData* g_pGameServerData;\n'
text += c[c.index('IGameServerData* g_pGameServerData'):c.index('static const Color CPU_COLOR')]
text += c[c.index('static void PutBoundedString'):c.index('TokenLine::TokenLine()')]
out.write_text(text, errors='surrogateescape')
