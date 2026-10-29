// Layout evidence uses rectangles extracted from the actual 1.1.1.13 SWF.
const fs=require('fs'),path=require('path'),assert=require('assert');
const root=path.join(__dirname,'../zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as');
const base=fs.readFileSync(path.join(root,'codo/views/InRoomBaseView.as'),'utf8');
const helper=fs.readFileSync(path.join(root,'codo/k33/LocalMatchControls.as'),'utf8');
const fixture=JSON.parse(fs.readFileSync(path.join(__dirname,'fixtures/match_room_bounds.json'),'utf8'));
function body(source,name) {
 const start=source.indexOf('function '+name+'(');assert(start>=0);let open=source.indexOf('{',start),depth=1,end=open+1;
 while(depth && end<source.length){if(source[end]==='{')depth++;if(source[end]==='}')depth--;end++;}
 return source.slice(open+1,end-1).replace(/var (\w+):[A-Za-z0-9_]+/g,'var $1');
}
function intersects(a,b){return a[0]<b[2] && a[2]>b[0] && a[1]<b[3] && a[3]>b[1];}
function inside(a,b){return a[0]>=b[0] && a[1]>=b[1] && a[2]<=b[2] && a[3]<=b[3];}
function scale(r,sx,sy){return [r[0]*sx,r[1]*sy,r[2]*sx,r[3]*sy];}
const width=Number(helper.match(/drawRect\(0,0,(\d+),30\)/)[1]);
const statusY=Number(helper.match(/this\.status\.y=(\d+)/)[1]);
const statusHeight=Number(helper.match(/this\.status\.height=(\d+)/)[1]);
for(const font of fixture.caption_font_metrics) {
 for(const [caption,pixels] of Object.entries(font.width_at_14px))assert(pixels+4<=244,font.locale+' '+font.font+': caption fits '+caption);
 assert(font.line_height_at_14px+4<=26,font.locale+' '+font.font+': caption height fits');
}
assert.equal(width,260);assert.equal(Number(helper.match(/this\.status\.width=(\d+)/)[1]),width);
assert.equal(Number(helper.match(/this\.caption\.width=(\d+)/)[1])+8,width-8);
assert(base.includes('this.localMatchControls.x = this.myInputTF.x;'));
assert(base.includes('this.localMatchControls.y = this.myChatTitleTF.y - 110;'));
assert(!/localMatchControls\.[xy]\s*=\s*\d/.test(base),'no guessed screen position');
function boundsObject(r){return {left:r[0],top:r[1],right:r[2],bottom:r[3],intersects(b){return intersects(r,[b.left,b.top,b.right,b.bottom]);}};}
global.LocalMatchControls=function(){this.x=0;this.y=0;this.getBounds=()=>boundsObject([this.x-0.5,this.y-0.5,this.x+width+0.5,this.y+statusY+statusHeight]);};
global.SyncDataID={IS_SHIP_BUILD:'ship'};
global.LocalizationRoom={START_DS:'',START_LS:'',INROOM_CHATTING:''};
global.LocalizationChatting={SEND_MESSAGE:''};
const init=new Function(body(base,'onInit'));
const resolutions=[[640,480],[800,600],[1024,768],[1280,720],[1280,960],[1920,1080],[2560,1440],[3440,1440]];
for(const [name,room] of Object.entries(fixture.rooms)) for(const ship of [true,false]) {
 const view={addChild:()=>{},getSyncDP:()=>ship};
 for(const [n,p] of Object.entries(room.positions))view[n]={x:p[0],y:p[1],visible:true};
 view.myDevPanel.getBounds=()=>boundsObject(room.visible_controls.myDevPanel);
 view.myMatchInfo.startVisualByState=()=>{};
 init.call(view);
 for(const [n,p] of Object.entries(room.positions))assert.deepEqual([view[n].x,view[n].y],p,name+': stock '+n+' unmoved');
 for(const n of ['myChatTitleTF','myChatBg','myScrollBar','myHistoryTF','myInputTF','mySendBtn'])assert.equal(view[n].visible,false,n+' reserved slot hidden');
 const x=view.localMatchControls.x,y=view.localMatchControls.y;
 const rect=[x-0.5,y-0.5,x+width+0.5,y+statusY+statusHeight];
 assert(rect[3]<=fixture.global_rooms_chat[1]-30,name+': clear above global Rooms chat');
 const controls=Object.fromEntries(Object.entries(room.visible_controls).filter(([n])=>!ship || !['myDevPanel','myDevDSRoomList','myRefreshDSBtn','myStartDSBtn','myStartLSBtn'].includes(n)));
 const visible={...controls,globalRoomsChat:fixture.global_rooms_chat};
 assert(inside(rect,fixture.stage),name+': inside stage');
 for(const [n,b] of Object.entries(visible))assert(!intersects(rect,b),name+': clear of '+n);
 for(const [n,b] of Object.entries(fixture.global_start_ready))assert(!intersects(rect,b),name+': clear of Start/Ready layout '+n);
 // Both aspect-preserving letterbox scaling and stretched viewport scaling
 // preserve containment and separation: every stock/new control shares parent.
 for(const [w,h] of resolutions) {
  for(const [sx,sy] of [[w/1280,h/960],[Math.min(w/1280,h/960),Math.min(w/1280,h/960)]]) {
   const r=scale(rect,sx,sy);
   assert(inside(r,scale(fixture.stage,sx,sy)),name+': viewport '+w+'x'+h);
   for(const [n,b] of Object.entries(visible))assert(!intersects(r,scale(b,sx,sy)),name+': scaled collision '+n);
  }
 }
 console.log(name+(ship?' shipping':' developer')+': anchored '+x+','+y+'; total bounds '+rect.join(',')+'; existing controls unchanged and separate');
}
// Reproduce the reported defect: old right-hand position intersects actual controls.
const old=[899.5,789.5,1222.5,868];
assert(intersects(old,fixture.rooms.inRoomView.visible_controls.myChangeForceBtn));
assert(intersects(old,fixture.rooms.inRoomView_cyborg.visible_controls.myForce_0));
for(const room of Object.values(fixture.rooms)){const p=room.positions.myInputTF,t=room.positions.myChatTitleTF;assert(intersects([p[0],t[1],p[0]+260,t[1]+78],fixture.global_rooms_chat),'previous hidden-roomchat anchor really collides with global Rooms chat');}
console.log('Room toggle geometry: MP/Survival and Zombies, both Start/Ready layouts, eight viewport sizes pass');
