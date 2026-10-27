from pathlib import Path
import re,json,hashlib,subprocess,shutil


def method(s,name):
 m=re.search(r'function\s+'+re.escape(name)+r'\s*\(([^)]*)\)\s*:\s*[\w.<>]+\s*\{',s);assert m,name
 p=m.end();start=p;depth=1;quote=None;escape=False
 while depth:
  ch=s[p]
  if quote:
   if escape:escape=False
   elif ch=='\\':escape=True
   elif ch==quote:quote=None
  elif ch in ('"',"'"):quote=ch
  elif ch=='{':depth+=1
  elif ch=='}':depth-=1
  p+=1
 body=s[start:p-1];body=re.sub(r'\bvar\s+(\w+)\s*:\s*[\w.<>]+',r'var \1',body);body=re.sub(r'\s+as\s+(Class|FriendObj|UserObj)','',body)
 return dict(params=[x.strip().split(':')[0] for x in m[1].split(',') if x.strip()],body=body)
SCRIPT=r'''
const assert=require('assert'),M=METHODS;let checks=0;
const constants=new Proxy({}, {get:(o,k)=>k});const BCEvent=constants,AsyncTaskID=constants,ViewState=constants,LocalizationDict=constants,SyncDataID=constants,Logger={print(){}};
const TITLE_ICON_INDEX={},POS_ORIGIN_X=0,POS_ORIGIN_Y=0,KeyboardEvent=constants;
class Panel {init(){} }
const getDefinitionByName=()=>Panel;
const scope={BCEvent,AsyncTaskID,ViewState,LocalizationDict,SyncDataID,Logger,TITLE_ICON_INDEX,POS_ORIGIN_X,POS_ORIGIN_Y,KeyboardEvent,getDefinitionByName};
function fn(cls,name){let m=M[cls][name];return new Function('scope',...m.params,'with(scope){'+m.body+'}');}
function invoke(cls,name,view,...args){return fn(cls,name).call(view,scope,...args);}
for(const lang of ['english','chinese']){
 const bad=new Proxy({}, {get(){throw Error('disabled feature read provider/native/UI');}});
 for(const name of ['canInviteToParty','canInviteToGame'])for(const data of [null,{},bad]){assert.strictEqual(invoke('RightClickMenuController',name,bad,data),false);checks++;}
 for(const [cls,names] of Object.entries(NOOPS))for(const name of names)for(const data of [null,{},bad]){invoke(cls,name,bad,data);checks++;}
 const calls=[],view={addChild(x){calls.push(['add',x]);},removeChild(x){calls.push(['remove',x]);},broadcast(...x){calls.push(['broadcast',...x]);},showView(){calls.push(['show']);},myTitleCategoryIcon:{stop(){},gotoAndStop(){}},myTitleTF:{},myBg:{},myBgGrow:{},stage:{addEventListener(){}},_currentSelectedBtn:null,_currentSelectedView:null};
 for(const n of ['Mail','Friend','Message','Party','Clan','QT'])view['myBtn'+n]={enable:true,selected:false,toString:()=>n};
 invoke('SocialSideBarView','onInit',view);assert.strictEqual(view.myBtnParty.enable,false);checks++;
 for(const n of ['Mail','Friend','Message','Clan','QT']){assert.strictEqual(view['myBtn'+n].enable,true);checks++;}
 for(const type of ['SHOW_PARTY_CREATOR','SHOW_PARTY_LIST'])for(const para of [undefined,{}, {refresh:true}]){
  const selected={state:'stay'},button={selected:true};view._currentSelectedView=selected;view._currentSelectedBtn=button;const before=calls.length;
  invoke('SocialSideBarView','onShowSocialChildPanel',view,{type,myPara:para});assert.strictEqual(view._currentSelectedView,selected);assert.strictEqual(view._currentSelectedBtn,button);assert.equal(calls.length,before);checks+=3;
 }
 const before=calls.length;invoke('SocialSideBarView','changeSelectedChildPanelByBtn',view,view.myBtnParty);assert.equal(calls.length,before);checks++;
 for(const [event,v,b] of [['SHOW_MAIL','_mailView','myBtnMail'],['SHOW_FRIENDLIST','_friendListView','myBtnFriend'],['SHOW_MESSAGEBOX','_messageBoxView','myBtnMessage'],['SHOW_CLAN_CREATOR','_clanCreatorView','myBtnClan'],['SHOW_CLAN_LIST','_clanListView','myBtnClan']]){
  view._currentSelectedView=null;view._currentSelectedBtn=null;invoke('SocialSideBarView','onShowSocialChildPanel',view,{type:event,myPara:{}});assert.strictEqual(view._currentSelectedView,view[v]);assert.strictEqual(view._currentSelectedBtn,view[b]);assert.equal(view[v].state,'ACTIVATED');checks+=3;
 }
 const actions=[],user={id:'foreign-id',name:'friend',isOnline:true},rv={broadcast(...a){actions.push(a);},isMe(){return false;},isMyFriend(){return false;},getSyncDP(id){return id==='PLAYER_NAME'?'self-name':true;},joinFriendGame(u){actions.push(['game',u]);},joinFriendParty(u){actions.push(['party',u]);}};
 assert.strictEqual(invoke('RightClickMenuController','canInviteToFriend',rv,user),true);assert.strictEqual(invoke('RightClickMenuController','canInviteToPrivateChat',rv,user),true);checks+=2;
 invoke('RightClickMenuController','onAddToFriendHandle',rv,user);assert.equal(actions.at(-1)[0],'EVT_ADD_FRIEND');assert.equal(actions.at(-1)[1].value,user.id);checks+=2;
 invoke('RightClickMenuController','onInviteToPrivateChatHandle',rv,user);assert.equal(actions.at(-1)[0],'EVT_START_PRIVATE_CHAT');assert.strictEqual(actions.at(-1)[1],user);checks+=2;
 user.isInRoom=true;invoke('RightClickMenuController','onFriendQuickJoinHandle',rv,{myPara:user});assert.equal(actions.at(-1)[0],'game');checks++;
 user.isInRoom=false;user.isInParty=true;invoke('RightClickMenuController','onFriendQuickJoinHandle',rv,{myPara:user});assert.equal(actions.at(-1)[0],'party');checks++;
}
console.log(JSON.stringify({passed:true,checks,locales:['english','chinese'],actual_AS_bodies:true,native_GF_x_execution:false}));
'''

CONFIG = {"codo.controllers.RightClickMenuController": {"relative": "as/codo/controllers/RightClickMenuController.as", "methods": ["onInviteToPartyHandle", "onInviteToGameHandle", "onPartyJoinedHandle", "canInviteToParty", "canInviteToGame"]}, "codo.controllers.PartyController": {"relative": "as/codo/controllers/PartyController.as", "methods": ["onProposePartyMembershipHandle", "onInviteUsersToPartyHandle", "onTryShowInviteListHandle", "onCreatePartyHandle", "createParty", "onTryOpenPartyHandle"]}, "codo.controllers.MatchController": {"relative": "as/codo/controllers/MatchController.as", "methods": ["onProposeRoomMembershipHandle"]}, "codo.views.SocialSideBarView": {"relative": "as/codo/views/SocialSideBarView.as", "methods": ["onInit", "onShowSocialChildPanel", "changeSelectedChildPanelByBtn"]}}
EXTRA = ["canInviteToFriend", "canInviteToPrivateChat", "onAddToFriendHandle", "onInviteToPrivateChatHandle", "onFriendQuickJoinHandle"]
NOOPS = {"RightClickMenuController": ["onInviteToPartyHandle", "onInviteToGameHandle", "onPartyJoinedHandle"], "PartyController": ["onProposePartyMembershipHandle", "onInviteUsersToPartyHandle", "onTryShowInviteListHandle", "onCreatePartyHandle", "createParty", "onTryOpenPartyHandle"], "MatchController": ["onProposeRoomMembershipHandle"]}
def execute(root, mutation=False):
    base=Path(root)/'zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as'
    methods={}
    for cls,d in CONFIG.items():
        text=(base/Path(*cls.split('.')).with_suffix('.as')).read_text(encoding='utf-8')
        if mutation and cls.endswith('RightClickMenuController'):
            old=method(text,'canInviteToParty')['body']
            text=text.replace(old,"throw Error('disabled invitation gate removed');",1)
        names=d['methods']+(EXTRA if cls.endswith('RightClickMenuController') else [])
        methods[cls.rsplit('.',1)[1]]={n:method(text,n) for n in names}
    script=SCRIPT.replace('METHODS',json.dumps(methods)).replace('NOOPS',json.dumps(NOOPS))
    return subprocess.run([shutil.which('node'),'-e',script],capture_output=True,text=True,encoding='utf-8')
