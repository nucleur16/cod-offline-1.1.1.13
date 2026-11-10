"""Closed one-selector readback, layered over unchanged two-factor checker."""
import re
def need(v,s):
    if not v:raise ValueError(s)
def body(text):
    need(type(text)is str and len(text.encode())<=262144,'bounded class export')
    matches=list(re.finditer(r'private\s+function\s+listPlayListNew\s*\(\s*\)\s*:\s*Array\s*\{',text))
    need(len(matches)==1,'exact selector signature');start=pos=matches[0].end();depth=1
    while depth:
        need(pos<len(text),'complete selector');ch=text[pos];pos+=1;depth+=(ch=='{')-(ch=='}')
    return text[start:pos-1]
def verify(text):
    b=body(text)
    # Exact original debug variable names are retained in this DATA transform.
    # Permit only redundant grouping parentheses around the retained AND.
    compact=re.sub(r'\s+','',b)
    prefix='varchoices:Array=this.listPlayList(0,PlayListType.COMBAT,true).concat(this.listPlayList(0,PlayListType.RANKED,true));varvisiblePlaylists:Array=[];foreach(varchoiceinchoices){if('
    suffix='){visiblePlaylists.push(choice);}}returnvisiblePlaylists;'
    predicates=['choice!=null&&(choice.type==PlayListType.COMBAT&&choice.combatTrainingType==CombatType.CT)',
                'choice!=null&&choice.type==PlayListType.COMBAT&&choice.combatTrainingType==CombatType.CT']
    need(compact in[prefix+p+suffix for p in predicates],'exact null/COMBAT/CT selector without ordinal condition')
    return dict(selector_signature=True,semantic_training_only=True,no_id_or_name_filter=True,order_and_identity=True)
