const assert=require('assert'), methods=METHODS;
const cache={},CodoPageViewDB=new Proxy({}, {get(t,k){
 if(!(k in cache)) cache[k]={id:k};
 if(!k.startsWith('PAGE_LAYER_')) cache[k].owner=CodoPageViewDB[k==='PAGE_POPUP_SUMMARY'||k==='PAGE_QUIT'?'PAGE_LAYER_POPUP':'PAGE_LAYER_MAIN'];
 return cache[k];
}});
const names=new Proxy({}, {get:(t,k)=>k}),SyncDataID=names,BCEvent=names,BCEventCommon=names;
const Logger={print(){}},UILogUtil={UILog_currentPageID(){}},Common={},CommonPageID={},SoundUtil={};
let firstLogin=false,ranked=false,quits=0,shown=[];
const CFunctionContainer={AS_IsFirstLogin(){return firstLogin;}},CommonUtil={quit(){quits++;}};
const PageManagerMain={getInstance(){return {showPage(page){shown.push(page);}};}};
const navigate=new Function('cPageObj','cParams','cCallBeforeGoto','cCallAfterGoto',methods.navigate);
let scenarios=0;
for(firstLogin of [false,true]) for(ranked of [false,true]){
 quits=0;shown=[];const events=[];
 const v={_currentMainPageObj:CodoPageViewDB.PAGE_MAIN_HOME,
  getSyncDP(){return ranked;},broadcast(evt){events.push(evt);},
  gotoPage(page){navigate.call(this,page,null,null,null);}};
 new Function('cEvt',methods.request).call(v,{});
 assert.equal(quits,0,'quit request bypassed confirmation');
 assert.deepEqual(shown,[CodoPageViewDB.PAGE_POPUP_SUMMARY]);
 new Function('cEvt',methods.back).call(v,{});
 assert.equal(events.at(-1),BCEventCommon.EVT_CLOSE_SUMMARY);
 assert.equal(quits,0,'Back to Game must not quit');
 new Function('cEvt',methods.confirm).call(v,{});
 assert.equal(events.at(-1),BCEventCommon.EVT_QUIT_GAME);
 // The terminal PAGE_QUIT route remains available to the confirmed event handler.
 v.gotoPage(CodoPageViewDB.PAGE_QUIT);assert.equal(quits,1);scenarios++;
}
console.log(JSON.stringify({scenarios,actual_AS_bodies:true,native_GF_x_execution:false}));
