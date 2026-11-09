#!/usr/bin/env python3
from cpp_tokens import tokens
padding=b'int pad; '*400
one=padding+b'''void f() { auto n=1'000'123u; auto s=R"tag(raw ' quote\n and ")tag"; const char* m="The 'unique' algorithm"; }'''
two=padding+b'''void f() {\n auto n = 1'000'123u;\n auto s = R"tag(raw ' quote\n and ")tag";\n const char* m = "The 'unique' algorithm";\n}'''
assert tokens(one)==tokens(two),'numeric/raw literal whitespace control'
assert tokens(one)!=tokens(two.replace(b'algorithm',b'algorithm changed')),'literal content cannot be ignored'
assert tokens(padding+b'a++;')!=tokens(padding+b'a+ +;'),'punctuation token boundaries matter'
for bad in [b'"unterminated',b'"invalid\nordinary"',b'R"bad(unclosed']:
    try:
        tokens(padding+bad)
    except AssertionError:
        pass
    else:
        raise AssertionError('unknown report accepted')
print('PASS C++ numeric separator/raw string/quoted content/punctuation/unknown controls')
