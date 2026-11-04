// Creator geometry comes from patch_ui_mp's exact 1.1.1.13 SWF export.
const fs=require('fs'),path=require('path'),assert=require('assert');
const root=path.join(__dirname,'../zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as');
const creator=fs.readFileSync(path.join(root,'codo/views/MatchMiniRoomCreatorView.as'),'utf8');
const option=fs.readFileSync(path.join(root,'codo/k33/LocalCreateRoomOption.as'),'utf8');
const mpct=fs.readFileSync(path.join(root,'codo/views/matchmpctroomcreator_sub/MatchMPCTRoomCreatorView_mpcreator.as'),'utf8');
const bridge=fs.readFileSync(path.join(root,'codo/k33/LocalMatchControls.as'),'utf8');
const stock={
  mp:{lastRow:[15,566.3,339.5,611.65],dropdown:[168,577.3,324,912.3]},
  zombies:{difficulty:[-6,467,364,523.4],dropdown:[168,577.3,324,912.3]},
  create:[178,626,343.35248,677.930847],
  stage:[0,0,1280,960],
};
function intersects(a,b){return a[0]<b[2]&&a[2]>b[0]&&a[1]<b[3]&&a[3]>b[1];}
function scale(r,x,y){return [r[0]*x,r[1]*y,r[2]*x,r[3]*y];}
assert(creator.includes('localCreateOption.x = this.myCreateBtn.x - 163;'));
assert(creator.includes('localCreateOption.y = this.myCreateBtn.y + 6;'));
assert(option.includes('drawRect(0,0,150,this.blueTheme ? 28 : 40)'));
assert(mpct.includes('this.localCreateOption.x = this.myOptionLabelTF_2.x + 528;'));
assert(mpct.includes('this.localCreateOption.y = this.myOptionLabelTF_2.y + 70;'));
assert(!option.includes('drawRoundRect'),'no foreign framed button beside stock Create Room');
const rectangles={
 mp:[stock.create[0]-88-.5,stock.create[1]-270-.5,stock.create[0]-88+150+.5,stock.create[1]-270+40+.5],
 zombies:[stock.create[0]-163-.5,stock.create[1]+6-.5,stock.create[0]-163+150+.5,stock.create[1]+6+40+.5]
};
assert.deepEqual(rectangles.mp,[89.5,355.5,240.5,396.5]);
assert.deepEqual(rectangles.zombies,[14.5,631.5,165.5,672.5]);
for(const [name,controls] of Object.entries({mp:[stock.mp.lastRow,stock.mp.dropdown],zombies:[stock.zombies.difficulty,stock.zombies.dropdown]})){
 const rectangle=rectangles[name];
 assert(!intersects(rectangle,stock.create),name+': clear of Create Room button');
 for(const c of controls)assert(!intersects(rectangle,c),name+': clear of existing options');
 for(const [w,h] of [[640,480],[800,600],[1024,768],[1280,720],[1280,960],[1920,1080],[2560,1440],[3440,1440]]){
  for(const [sx,sy] of [[w/1280,h/960],[Math.min(w/1280,h/960),Math.min(w/1280,h/960)]]){
   const r=scale(rectangle,sx,sy);assert(r[0]>=0&&r[1]>=0&&r[2]<=w&&r[3]<=h,name+': inside '+w+'x'+h);
   for(const c of controls)assert(!intersects(r,scale(c,sx,sy)),name+': viewport collision');
  }
 }
}
assert(creator.includes('this.myCreateBtn.x - 88') && creator.includes('this.myCreateBtn.y - 270'));
assert(option.includes('this.blueTheme ? (this.chosen ? 0x72D3DE : 0x43899F)'),'MP uses the blue creator palette');
assert(bridge.includes('this.visible=false;')&&!bridge.includes('drawRect'),'room overlay removed');
console.log('Create Room Local Play option clears stock MP/Zombies rows, dropdowns and Create button across eight viewports');
