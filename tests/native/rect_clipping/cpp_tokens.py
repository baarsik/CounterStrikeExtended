"""Strict scalar C++ preprocessing-token reader for source-preservation checks."""
import re

def tokens(data):
    # C++ preprocessing tokens: numeric separators must not open a char literal.
    result=[];index=0;size=len(data)
    raw_pattern=re.compile(rb'(?:u8|u|U|L)?R"')
    punctuation=[b'%:%:',b'>>=',b'<<=',b'->*',b'...',b'##',b'::',b'.*',b'->',b'++',b'--',b'<<',b'>>',
        b'<=',b'>=',b'==',b'!=',b'&&',b'||',b'+=',b'-=',b'*=',b'/=',b'%=',b'^=',b'&=',b'|=',b'<:',b':>',b'<%',b'%>',b'%:']
    while index<size:
        if data[index:index+1].isspace():
            index+=1;continue
        start=index
        raw=raw_pattern.match(data,index)
        if raw:
            begin=raw.end();opening=data.find(b'(',begin)
            assert opening>=begin and opening-begin<=16,'unknown raw-string delimiter'
            delimiter=data[begin:opening]
            assert not any(chr(value).isspace() or value in b'\\()' for value in delimiter),'invalid raw delimiter'
            closing=data.find(b')'+delimiter+b'"',opening+1)
            assert closing>=0,'unterminated raw literal'
            index=closing+len(delimiter)+2
        elif 48<=data[index]<=57 or (data[index:index+1]==b'.' and index+1<size and 48<=data[index+1]<=57):
            index+=1
            while index<size:
                byte=data[index]
                if chr(byte).isalnum() or byte in b"_.'":
                    index+=1
                elif byte in b'+-' and data[index-1] in b'eEpP':
                    index+=1
                else:
                    break
        elif data[index] in (34,39):
            quote=data[index];index+=1
            while index<size and data[index]!=quote:
                assert data[index] not in (10,13),'unknown multiline ordinary literal'
                index+=2 if data[index]==92 else 1
            assert index<size,'unterminated ordinary literal'
            index+=1
        elif chr(data[index]).isalpha() or data[index] in b'_$':
            index+=1
            while index<size and (chr(data[index]).isalnum() or data[index] in b'_$'):
                index+=1
        else:
            operator=next((value for value in punctuation if data.startswith(value,index)),None)
            index+=len(operator) if operator else 1
        result.append(data[start:index])
    assert len(result)>1000,'unknown/empty whole-TU preprocessor report'
    return result
