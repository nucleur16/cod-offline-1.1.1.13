"""Strict finite two-axis target proof; no compiler or runtime execution."""
import re

def center_semantics(V,before,after,candidate_expanded):
    import symbols as Y
    A=V.A;need=V.need
    old,new=V.unique39(before),V.unique39(after)
    a,b=A.method(old,'k33SetLabel'),A.method(new,'k33SetLabel')
    need(a['exceptions']==b['exceptions']==0 and a['init_scope']==b['init_scope'] and a['max_scope']==b['max_scope'],'target exception/scope drift')
    need(a['regs']==4 and b['regs']==6 and a['max_stack']==3 and b['max_stack']==4,'exact vertical target local/stack budget')
    oi,od=V.decode(a['code']);ni,nd=V.decode(b['code'])
    def prop(name):
        ids=[]
        for i in range(1,len(old['values']['multiname'])):
            try:
                if Y.mn(old,i)==(7,(22,''),name):ids.append(i)
            except ValueError:pass
        need(len(ids)==1,'unique original property '+name);return ids[0]
    text,x,width,y,height=(prop(n) for n in ('text','x','width','y','height'))
    first=next(i for i,r in enumerate(oi) if r==('setproperty',text));guard=first-2
    need(oi[guard:first]==[('getlocal',3),('getlocal',2)],'old setter boundary')
    need(len(ni)>guard and sum(op in ('ifne','iffalse') for op,_ in ni)==3,'exact branch count')
    for i,(op,val) in enumerate(oi[:guard]):
        expected=(op,len(ni)-1 if op in ('ifne','iffalse') and val==len(oi)-1 else val)
        need(ni[i]==expected,'null/text-equality guard prefix changed')
    def capture(position,size,local,conversion):
        return [('getlocal',3),('getproperty',position),('getlocal',3),('getproperty',size),('byte',2),('divide',None),('add',None)]+conversion+[('setlocal',local)]
    def restore(position,size,local):
        return [('getlocal',3),('getlocal',local),('getlocal',3),('getproperty',size),('byte',2),('divide',None),('subtract',None),('setproperty',position)]
    conversions=[[],[('convert_d',None)]]
    for i in range(1,len(old['values']['multiname'])):
        try:
            if A.qname(old,i)=='Number':conversions.append([('coerce',i)])
        except ValueError:pass
    setter=[('getlocal',3),('getlocal',2),('setproperty',text)]
    need(any(ni[guard:]==capture(x,width,4,cx)+capture(y,height,5,cy)+setter+restore(x,width,4)+restore(y,height,5)+[('return',None)] for cx in conversions for cy in conversions),'exact saved x/y centers BEFORE single setter and restored centers AFTER required')
    need(nd[:len(od)]==od and len(nd)==len(od)+2,'existing debug declarations changed')
    expected_line=next(i for i,s in enumerate(candidate_expanded.splitlines(),1) if re.search(r'\bvar w:Number\s*=',s))
    for (kind,index,reg,line),name,register in zip(nd[-2:],('w','h'),(3,4)):
        need(kind==1 and reg==register and new['values']['string'][index]==name and line==expected_line,'exact center debug locals')
    for name,pool in old['pools'].items():need(new['pools'][name]==pool,'every ORIGINAL pool exact')
    return dict(target='k33SetLabel',guard_prefix_exact=True,old_bodies=39,new_bodies=39,other_bodies_exact=38,locals4_to6=True,stack3_to4=True,exceptions0=True,both_centers_before_single_setter=True,both_centers_restored=True)

def exported(V,text):
    need=V.need
    need(type(text)is str and len(text.encode())<=262144,'bounded exported source')
    need(re.search(r'package\s+codo\.views\s*\{',text) and re.search(r'\bclass\s+OptionView\b',text),'exact exported class')
    need(len(re.findall(r'\bfunction\s+k33SetLabel\s*\(',text))==1,'unique exported setter')
    params,body=V.function_source(text);decls=re.findall(r'\bvar\s+(\w+)\s*:\s*(TextField|Number)\b',body)
    fields=[n for n,t in decls if t=='TextField'];centers=[n for n,t in decls if t=='Number']
    need(len(fields)==1 and len(centers)==2 and len(set(params+tuple(fields+centers)))==5,'exact three distinct setter locals')
    names={params[0]:'b',params[1]:'s',fields[0]:'tf',centers[0]:'cx',centers[1]:'cy'}
    actual=[names.get(t,t) for t in V.tokens(body)];variants=[]
    for ht in (False,True):
        for hx in (False,True):
            for hy in (False,True):
                prefix=('var tf:TextField=null;'if ht else'')+('var cx:Number=NaN;'if hx else'')+('var cy:Number=NaN;'if hy else'')
                source=prefix+'if(b==null){return;}'+('tf='if ht else'var tf:TextField=')+'b.getChildByName("label") as TextField;if(tf!=null&&tf.text!=s){'+('cx='if hx else'var cx:Number=')+'tf.x+tf.width/2;'+('cy='if hy else'var cy:Number=')+'tf.y+tf.height/2;tf.text=s;tf.x=cx-tf.width/2;tf.y=cy-tf.height/2;}'
                variants.extend([V.tokens(source),V.tokens(source+'return;')])
    need(actual in variants,'exported vertical setter exact guard/order/arithmetic required')
    return dict(class_name='codo.views.OptionView',setter_finite_roundtrip=True,both_axes=True,other_methods_protected_by_ABC=True)
