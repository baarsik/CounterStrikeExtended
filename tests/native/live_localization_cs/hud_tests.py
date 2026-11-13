from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[3]
out = root / 'build/live-localization/cs-hud-tests'
out.mkdir(parents=True, exist_ok=True)
def function(s, signature):
    start=s.index(signature); opening=s.index('{',start); end=opening+1; depth=1
    while depth:
        depth += (s[end]=='{') - (s[end]=='}'); end+=1
    return s[start:end]+'\n'
s=(root/'src/cstrike/cl_dll/message.cpp').read_text(encoding='latin1')
methods='\n'.join(function(s,x) for x in (
    'int CHudMessage::MessageAdd( const char*', 'void CHudMessage::MessageAdd( client_textmessage_t*',
    'float CHudMessage::MessageEndTime(', 'void CHudMessage::RefreshLanguage('))
h=r'''
#include "localization_message_snapshot.h"
#include <cassert>
#include <cstring>
#include <string>
#include <cstdio>
#include <cstdlib>
using std::strlen;
struct client_textmessage_t {
 int effect; unsigned char r1,g1,b1,a1,r2,g2,b2,a2;
 float x,y,fadein,fadeout,holdtime,fxtime;
 const char* pName; const char* pMessage;
};
client_textmessage_t catalogue{0,255,255,255,255,0,0,0,0,0,0,0.1f,0.5f,5.0f,0,"Tip","Original"};
client_textmessage_t* TextMessageGet(const char* name) { return std::strcmp(name,"Tip")==0 ? &catalogue : nullptr; }
bool CS_IsCatalogueMessage(const client_textmessage_t* message) { return message==&catalogue; }
namespace vgui2 {
 using HFont = unsigned int;
 struct IScheme { HFont GetFont(const char* name,bool) { return std::strcmp(name,"CreditsFont")==0 ? 4 : 3; } };
 struct SchemeManager { IScheme scheme; int GetDefaultScheme() {return 0;} IScheme* GetIScheme(int) {return &scheme;} };
 SchemeManager* scheme() { static SchemeManager manager; return &manager; }
}
vgui2::HFont _font=1;
vgui2::HFont _Newfont=2;
struct { bool IsSpectateOnly() {return false;} } gEngfuncs;
struct {float m_flTime=10;} gHUD;
int g_iTeamNumber=0;
constexpr int HUD_ACTIVE=1;
struct CHudMessage {
 static constexpr int MESSAGE_SLOT_COUNT=16;
 struct client_message_t { client_textmessage_t* pMessage=nullptr; unsigned int font=0; int numArgs=0; };
 client_message_t m_pMessages[16]{};
 cse::localization::MessageSnapshot<client_textmessage_t> m_messageSnapshots[16];
 cse::localization::MessageSnapshot<client_textmessage_t> m_gameTitleSnapshot;
 float m_startTime[16]{};
 float m_messageEndTimes[16]{};
 struct {float time=0; client_textmessage_t* pMessage=nullptr;} m_parms;
 client_textmessage_t* m_pGameTitle=nullptr;
 int m_iFlags=0;
 int MessageAdd(const char*,float,int,unsigned int);
 void MessageAdd(client_textmessage_t*);
 float MessageEndTime(int) const;
 void RefreshLanguage();
};
'''+methods+r'''
int main() {
 CHudMessage owner;
 const int index=owner.MessageAdd("#Tip",10.0f,1,_font);
 assert(index==0);
 assert(catalogue.effect==0 && catalogue.holdtime==5.0f);
 assert(owner.m_pMessages[0].pMessage!=&catalogue);
 const float deadline=owner.m_messageEndTimes[0];
 const float hold=owner.m_pMessages[0].pMessage->holdtime;
 catalogue.pMessage="A much longer Polish hint than the original text";
 owner.RefreshLanguage();
 assert(std::strcmp(owner.m_pMessages[0].pMessage->pMessage,catalogue.pMessage)==0);
 assert(owner.m_pMessages[0].pMessage->effect==2);
 assert(owner.m_pMessages[0].pMessage->holdtime==hold);
 assert(owner.m_messageEndTimes[0]==deadline && owner.m_startTime[0]==10.0f);
 assert(owner.m_pMessages[0].font==3);
 {
  std::string text="#Tip";
  client_textmessage_t literal=catalogue;
  literal.pName="__NETMESSAGE__";literal.pMessage=text.c_str();
  owner.MessageAdd(&literal);
 }
 owner.RefreshLanguage();
 assert(std::strcmp(owner.m_pMessages[1].pMessage->pMessage,"#Tip")==0);
 assert(!owner.m_messageSnapshots[1].HasCatalogueSource());
}
'''
(out/'messages.cpp').write_text(h)
subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(root/'src/public'),str(out/'messages.cpp'),'-o',str(out/'messages')],check=True)
subprocess.run([str(out/'messages')],check=True)
# Compile the actual status parser and refresh method with current live-state providers.
s=(root/'src/cstrike/cl_dll/statusbar.cpp').read_text(encoding='latin1')
methods=function(s,'void CHudStatusBar::ParseStatusString(')+function(s,'void CHudStatusBar::RefreshLanguage(')
h=r'''
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#define _snprintf snprintf
float g_ColorYellow[3]{};
float* GetClientColor(int) {return g_ColorYellow;}
struct client_textmessage_t { const char* pMessage; };
const char* healthText="Health";
client_textmessage_t* TextMessageGet(const char*) {static client_textmessage_t text; text.pMessage=healthText; return &text;}
struct { const char* name="#LiteralPlayer";} g_PlayerInfoList[33];
struct {int health=0;float showhealth=0;} g_PlayerExtraInfo[33];
struct {
 struct { void pfnGetPlayerInfo(int,void*) {} } unused;
 void pfnGetPlayerInfo(int,void*) {}
} gEngfuncs;
struct {
 struct { void LocaliseTextString(const char* source,char* output,int size) {std::strncpy(output,source,size-1);output[size-1]=0;} } m_TextMessage;
 struct {int m_iPlayerLastPointedAt=1;} m_Health;
 float m_flTime=10;
} gHUD;
struct CHudStatusBar {
 char m_szStatusText[1][128]{};
 char m_szStatusBar[1][128]{};
 int m_iStatusValues[8]{1,1,77};
 float* m_pflNameColors[1]{};
 void ParseStatusString(int,bool=true);
 void RefreshLanguage();
};
'''+methods+r'''
int main() {
 CHudStatusBar owner;
 std::strcpy(owner.m_szStatusText[0],"1 %p1 %i2 %h0");
 g_PlayerExtraInfo[1].health=33;g_PlayerExtraInfo[1].showhealth=123;
 owner.RefreshLanguage();
 assert(std::strcmp(owner.m_szStatusBar[0]," #LiteralPlayer 77 Health")==0);
 healthText="Zdrowie";
 owner.RefreshLanguage();
 assert(std::strcmp(owner.m_szStatusBar[0]," #LiteralPlayer 77 Zdrowie")==0);
 assert(g_PlayerExtraInfo[1].health==33 && g_PlayerExtraInfo[1].showhealth==123);
 assert(gHUD.m_Health.m_iPlayerLastPointedAt==1 && owner.m_iStatusValues[2]==77);
}
'''
(out/'status.cpp').write_text(h)
subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(out/'status.cpp'),'-o',str(out/'status')],check=True)
subprocess.run([str(out/'status')],check=True)
