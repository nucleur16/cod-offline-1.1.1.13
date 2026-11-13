// Offline: no Local Play control is laid out anywhere in the front end. The Create Room dialogs and the room view
// used to place a box (LocalCreateRoomOption) beside stock controls; this asserts it is gone, so there is no
// geometry left to keep clear of the stock rows, dropdowns and the Create button.
const fs=require('fs'),path=require('path'),assert=require('assert');
const root=path.join(__dirname,'../zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as');
const read=p=>fs.readFileSync(path.join(root,p),'utf8');
const creator=read('codo/views/MatchMiniRoomCreatorView.as');
const mpct=read('codo/views/matchmpctroomcreator_sub/MatchMPCTRoomCreatorView_mpcreator.as');
const bridge=read('codo/k33/LocalMatchControls.as');
// every hand-written class of the movie except the retired box itself
function walk(dir,out){for(const f of fs.readdirSync(dir,{withFileTypes:true})){const p=path.join(dir,f.name);if(f.isDirectory())walk(p,out);else if(f.name.endsWith('.as'))out.push(p);}return out;}
const users=walk(root,[]).filter(p=>path.basename(p)!=='LocalCreateRoomOption.as').filter(p=>/LocalCreateRoomOption|K33UI_LOCAL_PLAY/.test(fs.readFileSync(p,'utf8')));
assert.deepStrictEqual(users.map(p=>path.relative(root,p)),[],'no class makes or names the Local Play box');
for(const [name,text] of [['MatchMiniRoomCreatorView',creator],['MatchMPCTRoomCreatorView_mpcreator',mpct]]){
 assert(!/localCreateOption/.test(text),name+': no option instance');
 assert(!/myCreateBtn\.x - (88|163)|myCreateBtn\.y - 270|myCreateBtn\.y \+ 6|myOptionLabelTF_2\.x \+ 492|myOptionLabelTF_2\.y \+ 65/.test(text),name+': the placements of the box are gone');
}
assert(bridge.includes('this.visible=false;')&&!/this\.visible=(?!false;)/.test(bridge)&&!/drawRect|addChild|graphics/.test(bridge),'the room bridge draws nothing and is never shown');
const anchor=bridge.slice(bridge.indexOf('function anchorBesideReadyArea('));
assert(!/[=;]/.test(anchor.slice(anchor.indexOf('{')+1,anchor.indexOf('}')).replace(/\/\/.*/g,'')),'the placement call of the room view moves nothing');
console.log('No Local Play control: Create Room dialogs and the room view place nothing; the room bridge is invisible');
