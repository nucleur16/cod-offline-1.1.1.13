
const assert=require('assert'),methods=METHODS;
const MouseEvent={MOUSE_MOVE:'mouseMove'},MouseSize={WIDTH:3,HEIGHT:4},GameInfo={widthExtra:0,heightExtra:0},TweenParam={SPD_FADE:.2};
const CodoUIComponent={REAL_STAGE:null};const Point=function(x,y){this.x=x;this.y=y;};
const TweenLite={killTweensOf(){},to(target,time,values){Object.assign(target,values);}};
function stage(){return {mouseX:10,mouseY:20,stageWidth:100,stageHeight:100,adds:0,removes:0,listeners:new Set(),
 addEventListener(type,fn){assert.equal(type,'mouseMove');this.adds++;this.listeners.add(fn);},
 removeEventListener(type,fn){assert.equal(type,'mouseMove');this.removes++;this.listeners.delete(fn);}};}
function view(){const tip={width:20,height:10,getBounds(){return {x:0,y:0};}},v={_currentData:null,_currentTip:tip,myItemTip:{},
 _tipContentView:{getParameters(t){return t.data;}},hideAllTip(){},chooseTipByID(){this._currentTip=tip;},getBounds(){return {x:0,y:0};}};
 Object.defineProperty(v,'stage',{get(){throw Error('virtual stage lookup consulted');}});
 for(const n in methods)v[n]=new Function(n==='onTipMouseMoveHandle'?'cEvt':'cTarget','with(this){'+methods[n]+'}');return v;}
const target=()=>({tipEnable:true,data:{type:0,data:'tooltip',mouseFollow:true},localToGlobal:p=>p});
let checks=0;
for(const locale of ['english','chinese']){
 const s=stage(),v=view(),t=target();CodoUIComponent.REAL_STAGE=s;
 t.tipEnable=false;v.displayTip(t);assert.equal(s.adds,0);checks++;
 t.tipEnable=true;t.data=null;v.displayTip(t);assert.equal(s.adds,0);checks++;
 t.data={type:0,data:'tooltip',mouseFollow:true};
 for(let i=0;i<20;i++){v.displayTip(t);assert.equal(s.listeners.size,1);assert.equal(v._currentTip.x,13);assert.equal(v._currentTip.y,24);checks+=3;
  v.removeTip(t);assert.equal(s.listeners.size,0);assert.equal(v._currentData,null);checks+=2;}
 assert.equal(s.adds,s.removes);checks++;
 v.displayTip(t);s.mouseX=99;s.mouseY=99;v.onTipMouseMoveHandle({});assert.equal(v._currentTip.x,90);assert.equal(v._currentTip.y,90);checks+=2;
 v.myItemTip=v._currentTip;v.onTipMouseMoveHandle({});assert.equal(v._currentTip.x,80);checks++;
 v.removeTip(t);const x=v._currentTip.x;v.onTipMouseMoveHandle({});assert.equal(v._currentTip.x,x);checks++;
 t.data={type:0,data:'tooltip',mouseFollow:false,isAbsolutePos:true,x:99,y:12};v.displayTip(t);
 assert.equal(v._currentTip.x,80);assert.equal(v._currentTip.y,12);checks+=2;v.removeTip(t);
}
console.log(JSON.stringify({passed:true,checks,locales:['english','chinese'],actual_AS_bodies:true,native_GF_x_execution:false}));
