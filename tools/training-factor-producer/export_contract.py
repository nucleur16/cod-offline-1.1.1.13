"""Closed two-factor decompiler readback; no execution of exported Source."""
import re
def need(ok,message):
    if not ok:raise ValueError(message)
NAMES=('getXPLeagueFactor','getGPLeagueFactor')
def tokens(s):
    pattern=r'[A-Za-z_$][\w$]*|\d+(?:\.\d+)?|!=|==|&&|[{}():;=.,]'
    out=[];end=0
    for m in re.finditer(pattern,s):
        need(not s[end:m.start()].strip(),'unknown export token');out.append(m[0]);end=m.end()
    need(not s[end:].strip(),'unknown export suffix');return out
def function(text,name):
    matches=list(re.finditer(r'private\s+function\s+'+re.escape(name)+r'\s*\(([^)]*)\)\s*:\s*Number\s*\{',text))
    need(len(matches)==1,'unique exact private Number factor');m=matches[0]
    params=[]
    for raw,typ in zip(m[1].split(','),('int','int','PlayListObj')):
        match=re.fullmatch(r'\s*(\w+)\s*:\s*'+typ+(r'\s*=\s*null\s*'if typ=='PlayListObj'else r'\s*'),raw)
        need(match is not None,'exact factor parameter ABI');params.append(match[1])
    need(len(m[1].split(','))==3,'three factor parameters')
    pos=m.end();start=pos;depth=1
    while depth:
        need(pos<len(text),'complete factor function');ch=text[pos];pos+=1
        if ch=='{':depth+=1
        if ch=='}':depth-=1
    return params,text[start:pos-1]
def verify(text):
    need(type(text)is str and len(text.encode())<=262144,'bounded exported class')
    need(re.search(r'package\s+codo\.controllers\s*\{',text)and re.search(r'\bclass\s+MatchController\b',text),'exact class')
    for method in NAMES:
        params,body=function(text,method)
        locals=re.findall(r'\bvar\s+(\w+)\s*:\s*Number\b',body);need(len(locals)==1 and len(set(params+locals))==4,'exact factor local')
        rename=dict(zip(params+locals,('a','b','p','factor')));actual=[rename.get(t,t)for t in tokens(body)]
        call='MatchService.getInstance().'+method+'(a,b)'
        variants=[]
        # Explicit Number() is the same retained original convert_d operation.
        for fallback in (call,'Number('+call+')'):
            for initializer in ('NaN','Number(NaN)'):
                source='var factor:Number='+initializer+';if(p!=null&&p.type==PlayListType.COMBAT){factor=1;}else if(p!=null&&p.type==PlayListType.RANKED){factor=1;}else{factor='+fallback+';}return factor;'
                variants.append(tokens(source))
        need(actual in variants,'exact null/COMBAT1/Ranked1/service fallback readback')
    return dict(two_factors=True,null_fallback_preserved=True,combat_full_rate=True,Ranked_full_rate=True,original_service_methods_args=True,display_name_independent=True)
