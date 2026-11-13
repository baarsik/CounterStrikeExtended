from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
out=root/'build/live-localization/cs-text-tests'
out.mkdir(parents=True,exist_ok=True)
def function(s,signature):
 start=s.index(signature); opening=s.index('{',start);end=opening+1;depth=1
 while depth:
  depth += (s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
s=(root/'src/cstrike/cl_dll/text_message.cpp').read_text(encoding='latin1')
body=function(s,'char* CHudTextMessage::LocaliseTextString(')
h=r'''
#include <cassert>
#include <cstring>
struct client_textmessage_t { const char* pMessage; };
client_textmessage_t* TextMessageGet(const char* name) {
 static client_textmessage_t message{"A translation much longer than the caller buffer"};
 return std::strcmp(name,"Long")==0 ? &message : nullptr;
}
struct CHudTextMessage { char* LocaliseTextString(const char*,char*,int); };
'''+body+r'''
int main() {
 CHudTextMessage owner;
 struct Buffer { char text[8]; char guard[8]; } buffer{};
 std::memset(buffer.guard,'Z',sizeof(buffer.guard));
 owner.LocaliseTextString("#Long!",buffer.text,sizeof(buffer.text));
 assert(std::strcmp(buffer.text,"A trans")==0);
 for(char ch:buffer.guard) { assert(ch=='Z'); }
 char tiny[1]={'X'};
 owner.LocaliseTextString("#Long",tiny,1);
 assert(tiny[0]==0);
 char literal[64]{};
 owner.LocaliseTextString("x#Missing y",literal,sizeof(literal));
 assert(std::strcmp(literal,"x#Missing y")==0);
 owner.LocaliseTextString(nullptr,literal,sizeof(literal));
 assert(literal[0]==0);
}
'''
(out/'text.cpp').write_text(h)
subprocess.run(['c++','-std=c++17','-fsanitize=address,undefined',str(out/'text.cpp'),'-o',str(out/'text')],check=True)
subprocess.run([str(out/'text')],check=True)
