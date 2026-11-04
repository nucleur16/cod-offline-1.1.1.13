// Execute the actual stored/exported AS handlers across menu and match transitions.
const fs=require('fs'),path=require('path'),assert=require('assert');
const file=process.argv[2] || path.join(__dirname,'../zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as/codo/views/ServerMessageView.as');
const source=fs.readFileSync(file,'utf8');
function body(name){
 const start=source.indexOf('function '+name+'(');assert(start>=0,name);
 const begin=source.indexOf('{',start);let end=begin+1,depth=1;
 while(depth){depth+=(source[end]==='{')-(source[end]==='}');end++;}
 return source.slice(begin+1,end-1).replace(/var (\w+):\w+/g,'var $1');
}
function constant(name){const match=source.match(new RegExp(name+':(?:Number|int) = (\\d+)'));assert(match,name);return Number(match[1]);}
global.POS_ORIGIN_X=constant('POS_ORIGIN_X');global.POS_ORIGIN_Y=constant('POS_ORIGIN_Y');
global.POS_X_START=372;global.POS_X_END=4;global.PIXELS_PER_SECOND=75;
global.TextFieldAutoSize={LEFT:'left'};global.Linear={easeNone:0};
global.TweenLite={to:()=>{}};
global.ServerMessageType={IDIP:1,TRY_ON:2};global.ServerMessageDuration={INFINITE:-1};
global.ServerMessageObj=function(obj){Object.assign(this,obj)};
global.Timer=function(){this.running=false;this.addEventListener=()=>{};this.start=()=>{this.running=true}};
global.TimerEvent={TIMER_COMPLETE:1};
function view(){
 const c={myMsgTF:{text:'',htmlText:'',width:240},visible:true,_serverMsgToShow:[],_currentServerMsgIndex:-1,_isInGame:false,isServerMsgRepetitive:()=>false,tryStopAnimation:()=>{}};
 for(const name of ['onInit','onSetIsInGameHandle','show','tryStartAnimation'])c[name]=new Function(name==='show'?'cServerMsg':name==='onSetIsInGameHandle'?'cEvt':'unused',body(name));
 return c;
}
const messages=['Welcome to CoDOL Classic','Service notice','Server queue 1/2','Match server starting.'];
for(const message of messages){
 const c=view();c.onInit();assert.deepEqual([c.x,c.y],[880,80],'initial frontend');
 c.show({message,type:2,duration:10000});assert.deepEqual([c.x,c.y],[880,80],message);
 c.onSetIsInGameHandle({myPara:true});assert.deepEqual([c.x,c.y],[282,5],'existing in-game geometry');
 c.onSetIsInGameHandle({myPara:false});assert.deepEqual([c.x,c.y],[880,80],'return to menu');
 c.visible=false;c.show({message:'Return notice',type:2,duration:10000});assert(c.visible,'frontend notice restores visibility');
}
console.log('Ticker: four notice categories stay top right on initial frontend and return; in-game geometry preserved');
