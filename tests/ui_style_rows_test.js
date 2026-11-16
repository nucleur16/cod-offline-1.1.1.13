// The Button icons / HUD style rows, and the Language row that K33_STYLE_LANGUAGE_ROW switches off
// (docs/UI_STYLE_OPTIONS_20261112.md): runs the rows' actual ActionScript logic from both movies' sources --
// codoingame.gfx MainPause (pause menu, slider rows) and codo.gfx OptionView_video (Options > Graphics, drop-downs) --
// against a fake dvar table, and checks the wiring and the localize keys they rely on. The language logic is run with
// the switch on (it must keep working for the day the row comes back); what ships is checked with the switch as the
// sources have it: off, no Language row, no gap, no k33_language* dvar read for a row.
//   node tests/ui_style_rows_test.js
const fs=require('fs'),path=require('path'),assert=require('assert');
const root=path.join(__dirname,'..');
const read=p=>fs.readFileSync(path.join(root,p),'utf8');
const SOURCES={
 pause:read('zones/controller_ingame/rawfile/scaleform/default/codoingame.gfx.gfxpatch/as/MainPause.as'),
 video:read('zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as/codo/views/option_sub/OptionView_video.as'),
};

// --- the shared logic is one text in both movies ---------------------------------------------------------------
function shared(text){
 const a=text.indexOf('// k33 style shared: begin'),b=text.indexOf('// k33 style shared: end');
 assert(a>=0&&b>a,'shared block markers');
 return text.slice(a,b);
}
assert.strictEqual(shared(SOURCES.pause),shared(SOURCES.video),'the k33Style* logic must be the same text in both movies');
// the one switch of the Language row is in that shared text, and it is on
const LANGUAGE_ROW=/private static const K33_STYLE_LANGUAGE_ROW:Boolean = (true|false);/.exec(shared(SOURCES.video));
assert(LANGUAGE_ROW,'K33_STYLE_LANGUAGE_ROW is declared once, in the shared block');
assert.strictEqual(LANGUAGE_ROW[1],'true','the Language row is switched on (the language zone reloads live: docs/LANGUAGE_SWITCH_20261112.md)');
for(const [n,t] of Object.entries(SOURCES))assert.strictEqual(t.split('K33_STYLE_LANGUAGE_ROW:Boolean').length,2,n+': one declaration of the switch');

// --- ActionScript -> JavaScript: a function's body with the type annotations taken off ---------------------------
function body(text,name){
 const start=text.indexOf('function '+name+'(');assert(start>=0,name);
 let open=text.indexOf('{',start),depth=1,end=open+1;
 while(depth&&end<text.length){if(text[end]==='{')depth++;if(text[end]==='}')depth--;end++;}
 const args=text.slice(text.indexOf('(',start)+1,text.indexOf(')',start)).split(',').map(a=>a.split(':')[0].trim()).filter(Boolean);
 const js=text.slice(open+1,end-1).replace(/var (\w+):[A-Za-z0-9_.*]+/g,'var $1').replace(/catch\((\w+):\w+\)/g,'catch($1)').replace(/ as [A-Z]\w*/g,'');
 return new Function(...args,js);
}
function literal(text,name){
 const at=text.indexOf('const '+name+':');assert(at>=0,name);
 const eq=text.indexOf('=',at),open=text[eq+2];assert(open==='['||open==='{',name);
 let depth=0,end=eq+2;
 do{if(text[end]==='['||text[end]==='{')depth++;if(text[end]===']'||text[end]==='}')depth--;end++;}while(depth);
 return new Function('return '+text.slice(eq+2,end))();
}
const SHARED=['k33StyleTrim','k33StyleDigits','k33StyleOptions','k33StyleName','k33StyleCurrent','k33StyleIndex','k33StyleStep','k33StyleSet','k33StyleSig'];

// --- a fake engine: dvars with the engine's own refusal of out-of-range ints, and a localize table ----------------
let dvars,ranges,sets,strings,reads;
global.int=v=>Math.trunc(Number(v))||0;
global.CFunctionContainerCommon={
 Dvar_GetString:n=>{reads.push(n);return dvars[n]===undefined?'':String(dvars[n]);},
 Dvar_GetInt:n=>{reads.push(n);return dvars[n]===undefined?0:(parseInt(dvars[n],10)||0);},
 setdvar:(n,v)=>{sets.push([n,v]);if(dvars[n]===undefined)return;            // a dvar the DLL does not have: nothing
  if(ranges[n]&&(Number(v)<ranges[n][0]||Number(v)>ranges[n][1]))return;      // out of range: the engine refuses it
  dvars[n]=v;},
};
global.LocalizeUtil={localize:k=>strings[k.slice(1)]===undefined?k:strings[k.slice(1)]};
function engine(over){
 dvars=Object.assign({k33_language:'en',k33_languageList:'en;zh',k33_languageState:'0',k33_languageSerial:'0',
  gpad_glyphs:'0',k33_padinfo:'0|1|Xbox 360 Controller|0|0',gpad_glyphSerial:'0',
  k33_hudStyle:'0',k33_hudStyleList:'0:stock;2:v26'},over||{});
 for(const k of Object.keys(dvars))if(dvars[k]===null)delete dvars[k];
 ranges={gpad_glyphs:[0,6],k33_hudStyle:[0,9]};sets=[];reads=[];
 strings={K33UI_LANG_NAME_EN:'English',K33UI_LANG_NAME_ZH:'中文',K33UI_HUD_STYLE_STOCK:'Classic (1.1.1.13)',K33UI_HUD_STYLE_V26:'Version 2.6',
  K33UI_GLYPHS_AUTO:'Auto',K33UI_GLYPHS_XBOX:'Xbox',K33UI_GLYPHS_PS:'PlayStation',K33UI_GLYPHS_NINTENDO:'Wii U / Switch',
  K33UI_GLYPHS_GENERIC:'Generic',K33UI_GLYPHS_TEXT:'Text only',K33UI_GLYPHS_KBM:'Keyboard & mouse'};
}
function logic(which,extra){
 const text=SOURCES[which],o={};
 for(const n of SHARED.concat(extra||[]))o[n]=body(text,n);
 return o;
}

global.K33_STYLE_LANGUAGE_ROW=true; // the language logic, as it is when the row is switched on
for(const which of ['pause','video']){
 const text=SOURCES[which];
 global.K33_GLYPH_NAMES=literal(text,'K33_GLYPH_NAMES');
 global.K33_STYLE_DVARS=literal(text,'K33_STYLE_DVARS');
 global.K33_STYLE_KINDS=literal(text,'K33_STYLE_KINDS');
 global.K33_STYLE_POLL=Number(/K33_STYLE_POLL:int = (\d+)/.exec(text)[1]);
 global.K33_STYLE_HOLD=Number(/K33_STYLE_HOLD:int = (\d+)/.exec(text)[1]);
 assert.deepStrictEqual(K33_GLYPH_NAMES,['AUTO','XBOX','PS','NINTENDO','GENERIC','TEXT','KBM'],'gpad_glyphs 0..6 in the contract order');
 assert.deepStrictEqual(K33_STYLE_DVARS,{lang:'k33_language',glyph:'gpad_glyphs',hud:'k33_hudStyle'},'the contract dvars');
 assert.deepStrictEqual(K33_STYLE_KINDS,['lang','glyph','hud'],'row order');
 const L=logic(which);
 const vs=o=>o.map(x=>x.v),keys=o=>o.map(x=>x.key);

 // list parsing
 engine();
 assert.deepStrictEqual(L.k33StyleOptions('lang'),[{v:'en',key:'K33UI_LANG_NAME_EN',raw:'en'},{v:'zh',key:'K33UI_LANG_NAME_ZH',raw:'zh'}]);
 assert.deepStrictEqual(L.k33StyleOptions('hud'),[{v:'0',key:'K33UI_HUD_STYLE_STOCK',raw:'stock'},{v:'2',key:'K33UI_HUD_STYLE_V26',raw:'v26'}]);
 assert.deepStrictEqual(vs(L.k33StyleOptions('glyph')),['0','1','2','3','4','5','6']);
 assert.deepStrictEqual(keys(L.k33StyleOptions('glyph')),K33_GLYPH_NAMES.map(n=>'K33UI_GLYPHS_'+n));
 engine({k33_languageList:' en ;; zh;en;pt-br; ',k33_hudStyleList:'0:stock; 2 : v26 ;x:bad;3;:v33;4:;02:dup;12:v35;1234567:big;-1:neg'});
 assert.deepStrictEqual(vs(L.k33StyleOptions('lang')),['en','zh','pt-br'],'blank and repeated ids dropped, ids trimmed');
 assert.strictEqual(L.k33StyleOptions('lang')[2].key,'K33UI_LANG_NAME_PT-BR');
 assert.deepStrictEqual(L.k33StyleOptions('hud').map(x=>x.v+'='+x.raw),['0=stock','2=v26','12=v35'],'malformed and repeated pairs dropped');

 // a missing or empty list: no row (an old Kernel33.dll), never an exception
 for(const gone of [null,'','  ',';;']){
  engine({k33_languageList:gone,k33_hudStyleList:gone,k33_padinfo:gone===null?null:''});
  for(const kind of K33_STYLE_KINDS)assert.deepStrictEqual(L.k33StyleOptions(kind),[],which+' '+kind+' without its list: '+JSON.stringify(gone));
 }

 // names: each language in its own name, the raw id for one without an entry
 engine({k33_languageList:'en;zh;de',k33_hudStyleList:'0:stock;7:v99'});
 assert.deepStrictEqual(L.k33StyleOptions('lang').map(o=>L.k33StyleName(o)),['English','中文','de']);
 assert.deepStrictEqual(L.k33StyleOptions('hud').map(o=>L.k33StyleName(o)),['Classic (1.1.1.13)','v99']);
 strings.K33UI_LANG_NAME_DE='';assert.strictEqual(L.k33StyleName(L.k33StyleOptions('lang')[2]),'de','an empty entry also shows the id');

 // the current value; one the list does not have shows the first choice
 engine({k33_language:'zh',k33_hudStyle:'2',gpad_glyphs:'5'});
 for(const [kind,want] of [['lang',1],['hud',1],['glyph',5]])assert.strictEqual(L.k33StyleIndex(L.k33StyleOptions(kind),L.k33StyleCurrent(kind)),want,kind);
 engine({k33_language:'fr',k33_hudStyle:'5',gpad_glyphs:'9'});
 for(const kind of K33_STYLE_KINDS)assert.strictEqual(L.k33StyleIndex(L.k33StyleOptions(kind),L.k33StyleCurrent(kind)),0,kind+': not listed -> first');
 engine({k33_language:null,k33_hudStyle:null,gpad_glyphs:null});
 for(const kind of K33_STYLE_KINDS)assert.strictEqual(L.k33StyleIndex(L.k33StyleOptions(kind),L.k33StyleCurrent(kind)),0,kind+': dvar missing -> first');
 engine({k33_language:'ZH'});assert.strictEqual(L.k33StyleIndex(L.k33StyleOptions('lang'),L.k33StyleCurrent('lang')),1,'ids compare without case');
 assert.strictEqual(L.k33StyleIndex([],'x'),0);

 // stepping wraps at both ends
 assert.deepStrictEqual([0,1,2].map(i=>L.k33StyleStep(i,3,1)),[1,2,0]);
 assert.deepStrictEqual([0,1,2].map(i=>L.k33StyleStep(i,3,-1)),[2,0,1]);
 assert.strictEqual(L.k33StyleStep(0,1,1),0);assert.strictEqual(L.k33StyleStep(0,1,-1),0);assert.strictEqual(L.k33StyleStep(0,0,1),0);
 assert.strictEqual(L.k33StyleStep(6,7,1),0);assert.strictEqual(L.k33StyleStep(0,7,-1),6);

 // sets go through setdvar, as strings, only when the value differs; out-of-range indexes set nothing
 engine();let opts=L.k33StyleOptions('hud');
 assert.strictEqual(L.k33StyleSet('hud',opts,0),false);assert.deepStrictEqual(sets,[],'no set when the dvar already holds it');
 assert.strictEqual(L.k33StyleSet('hud',opts,1),true);assert.deepStrictEqual(sets,[['k33_hudStyle','2']]);assert.strictEqual(dvars.k33_hudStyle,'2');
 assert.strictEqual(L.k33StyleSet('hud',opts,2),false);assert.strictEqual(L.k33StyleSet('hud',opts,-1),false);assert.strictEqual(sets.length,1);
 opts=L.k33StyleOptions('lang');assert.strictEqual(L.k33StyleSet('lang',opts,1),true);assert.deepStrictEqual(sets[1],['k33_language','zh']);
 opts=L.k33StyleOptions('glyph');assert.strictEqual(L.k33StyleSet('glyph',opts,6),true);assert.deepStrictEqual(sets[2],['gpad_glyphs','6']);
 assert(sets.every(s=>typeof s[1]==='string'),'setdvar(name, String(v))');

 // an old DLL whose gpad_glyphs stops at 5 refuses 6: what is read back is what the dvar still holds
 engine({gpad_glyphs:'5'});ranges.gpad_glyphs=[0,5];opts=L.k33StyleOptions('glyph');
 assert.strictEqual(L.k33StyleSet('glyph',opts,6),true);
 assert.strictEqual(L.k33StyleIndex(opts,L.k33StyleCurrent('glyph')),5,'the refused value is not what the row ends on');

 // the list signature changes when the list does
 engine();const sig=L.k33StyleSig(L.k33StyleOptions('hud'));
 dvars.k33_hudStyleList='0:stock;2:v26;4:v35';assert.notStrictEqual(L.k33StyleSig(L.k33StyleOptions('hud')),sig);
 assert.strictEqual(L.k33StyleSig([]),'');
}

// --- the pause menu's rows: pick, hold, read back, language serial, list change -----------------------------------
{
 const text=SOURCES.pause;
 global.K33_GLYPH_NAMES=literal(text,'K33_GLYPH_NAMES');global.K33_STYLE_DVARS=literal(text,'K33_STYLE_DVARS');
 global.K33_STYLE_KINDS=literal(text,'K33_STYLE_KINDS');
 global.K33_STYLE_POLL=Number(/K33_STYLE_POLL:int = (\d+)/.exec(text)[1]);global.K33_STYLE_HOLD=Number(/K33_STYLE_HOLD:int = (\d+)/.exec(text)[1]);
 function pause(){
  const p=logic('pause',['k33StyleOf','k33StyleShown','k33StyleShow','k33StylePick','k33StyleFrame','k33StepSlider','k33SliderRange']);
  const view={stage:{}};
  Object.assign(p,{k33StyleRows:[],k33StyleSync:false,k33StyleView:view,k33StyleAnchor:{},k33StyleSerial:0,k33StyleFrames:0,k33DragRow:null,
   k33FovRow:null,rebuilds:[],k33Shown:()=>true,k33StyleNoteUpdate(){this.notes=(this.notes||0)+1;},
   k33StyleRebuild(v,texts){this.rebuilds.push(texts);}});
  for(const kind of K33_STYLE_KINDS){
   const opts=p.k33StyleOptions(kind);if(!opts.length)continue;
   const slider={moves:0,myCurrentTF:{text:''},_v:NaN,get value(){return this._v;},set value(v){if(v!==this._v){this._v=v;this.moves++;this.myCurrentTF.text=Number(v).toFixed(1);}}};
   const rec={kind,row:{mySlider:slider,parent:view},opts,names:opts.map(o=>p.k33StyleName(o)),shown:-1,hold:0};
   p.k33StyleRows.push(rec);p.k33StyleShow(rec,p.k33StyleIndex(opts,p.k33StyleCurrent(kind)));
  }
  p.frames=n=>{for(let i=0;i<n;i++)p.k33StyleFrame(null);};
  p.rec=kind=>p.k33StyleRows.find(r=>r.kind===kind);
  return p;
 }
 engine({k33_language:'zh',gpad_glyphs:'2',k33_hudStyle:'2'});let p=pause();
 assert.deepStrictEqual(p.k33StyleRows.map(r=>[r.kind,r.shown,r.row.mySlider.myCurrentTF.text]),[['lang',1,'中文'],['glyph',2,'PlayStation'],['hud',1,'Version 2.6']],'rows show their dvars, by name');
 assert.deepStrictEqual(sets,[],'showing a row sets nothing');
 // pad right on HUD style (2 choices): wraps to the first and sets the listed number
 p.k33StepSlider(p.rec('hud').row,1);assert.deepStrictEqual(sets,[['k33_hudStyle','0']]);assert.strictEqual(p.rec('hud').row.mySlider.myCurrentTF.text,'Classic (1.1.1.13)');
 p.k33StepSlider(p.rec('hud').row,-1);assert.deepStrictEqual(sets[1],['k33_hudStyle','2'],'left from the first wraps to the last');
 // the slider range of a style row is one stop per choice
 assert.deepStrictEqual(p.k33SliderRange(p.rec('glyph').row),{min:0,max:6});assert.deepStrictEqual(p.k33SliderRange(p.rec('hud').row),{min:0,max:1});
 // a refused value (old DLL: gpad_glyphs 0..5) stays on show only for the hold, then the row shows the dvar again
 ranges.gpad_glyphs=[0,5];p.k33StylePick(p.rec('glyph'),6);
 assert.strictEqual(p.rec('glyph').shown,6);assert.strictEqual(p.rec('glyph').hold,K33_STYLE_HOLD);
 p.frames(K33_STYLE_POLL);assert.strictEqual(p.rec('glyph').shown,6,'held while the engine takes the set');
 p.frames(K33_STYLE_HOLD+K33_STYLE_POLL);assert.strictEqual(p.rec('glyph').shown,2,'what the dvar holds is shown');assert.strictEqual(p.rec('glyph').row.mySlider.myCurrentTF.text,'PlayStation');
 // a value changed elsewhere (console, the other menu) is followed
 dvars.k33_language='en';p.frames(K33_STYLE_POLL);assert.strictEqual(p.rec('lang').row.mySlider.myCurrentTF.text,'English');
 // a row being dragged is left alone
 p.k33DragRow=p.rec('lang').row;dvars.k33_language='zh';p.frames(K33_STYLE_POLL*2);assert.strictEqual(p.rec('lang').shown,0);p.k33DragRow=null;
 p.frames(K33_STYLE_POLL);assert.strictEqual(p.rec('lang').shown,1);
 assert.deepStrictEqual(p.rebuilds,[],'no rebuild without a serial or list change');
 // k33_languageSerial moved: the list is built again with its texts, once
 dvars.k33_languageSerial='1';p.frames(K33_STYLE_POLL*3);assert.deepStrictEqual(p.rebuilds,[true]);
 // a list that changed (or turned up) builds the rows again
 dvars.k33_hudStyleList='0:stock;2:v26;4:v35';p.frames(K33_STYLE_POLL);assert.deepStrictEqual(p.rebuilds,[true,false]);
 engine({k33_languageList:null});p=pause();assert.deepStrictEqual(p.k33StyleRows.map(r=>r.kind),['glyph','hud'],'no Language row without k33_languageList');
 p.frames(K33_STYLE_POLL*2);assert.deepStrictEqual(p.rebuilds,[]);dvars.k33_languageList='en;zh';p.frames(K33_STYLE_POLL);assert.deepStrictEqual(p.rebuilds,[false],'the row is added when its list turns up');
 // a closed menu costs nothing and changes nothing
 engine();p=pause();p.k33Shown=()=>false;dvars.k33_languageSerial='5';p.frames(K33_STYLE_POLL*4);assert.deepStrictEqual(p.rebuilds,[]);
 p.k33Shown=()=>true;p.frames(K33_STYLE_POLL);assert.deepStrictEqual(p.rebuilds,[true],'a switch made while it was closed is caught when it opens');
 // one choice only: the row shows it and never sets
 engine({k33_hudStyleList:'3:v33',k33_hudStyle:'0'});p=pause();assert.strictEqual(p.rec('hud').row.mySlider.myCurrentTF.text,'v33');
 p.k33StepSlider(p.rec('hud').row,1);p.k33StylePick(p.rec('hud'),5);assert.deepStrictEqual(sets,[['k33_hudStyle','3']],'the only listed style is what a pick sets');
}

// --- the pause menu with the Language row switched off (the switch still works when turned off) ------------------------------------------------
{
 global.K33_STYLE_LANGUAGE_ROW=false;
 for(const which of ['pause','video']){
  const L=logic(which);
  for(const list of ['en;zh','en','',null]){
   engine({k33_languageList:list});
   assert.deepStrictEqual(L.k33StyleOptions('lang'),[],which+': no Language choices while the row is off ('+JSON.stringify(list)+')');
   assert.deepStrictEqual(reads,[],which+': no dvar is read for the Language row');
  }
  engine();assert.strictEqual(L.k33StyleOptions('glyph').length,7);assert.strictEqual(L.k33StyleOptions('hud').length,2);
 }
 const text=SOURCES.pause;
 const p=logic('pause',['k33StyleOf','k33StyleShown','k33StyleShow','k33StylePick','k33StyleFrame','k33StepSlider','k33SliderRange','k33StyleNoteUpdate']);
 const view={stage:{}};
 engine({k33_language:'zh',gpad_glyphs:'2',k33_hudStyle:'2',k33_languageState:'2'});
 Object.assign(p,{k33StyleRows:[],k33StyleSync:false,k33StyleView:view,k33StyleAnchor:{},k33StyleSerial:0,k33StyleFrames:0,k33DragRow:null,
  k33FovRow:null,k33StyleNote:null,k33StyleNoteKey:'',rebuilds:[],k33Shown:()=>true,k33StyleRebuild(v,texts){this.rebuilds.push(texts);}});
 for(const kind of K33_STYLE_KINDS){
  const opts=p.k33StyleOptions(kind);if(!opts.length)continue;
  const slider={myCurrentTF:{text:''},_v:NaN,get value(){return this._v;},set value(v){this._v=v;}};
  const rec={kind,row:{mySlider:slider,parent:view},opts,names:opts.map(o=>p.k33StyleName(o)),shown:-1,hold:0};
  p.k33StyleRows.push(rec);p.k33StyleShow(rec,p.k33StyleIndex(opts,p.k33StyleCurrent(kind)));
 }
 assert.deepStrictEqual(p.k33StyleRows.map(r=>[r.kind,r.row.mySlider.myCurrentTF.text]),[['glyph','PlayStation'],['hud','Version 2.6']],'pause: Button icons and HUD style only, in that order');
 reads=[];for(let i=0;i<K33_STYLE_POLL*4;i++)p.k33StyleFrame(null);
 assert.deepStrictEqual(p.rebuilds,[],'pause: a language list does not bring the row back');
 assert.deepStrictEqual([...new Set(reads.filter(n=>n.indexOf('k33_language')===0))],['k33_languageSerial'],'pause: of the language dvars only the serial is read (to translate the rows again)');
 dvars.k33_languageSerial='4';for(let i=0;i<K33_STYLE_POLL;i++)p.k33StyleFrame(null);
 assert.deepStrictEqual(p.rebuilds,[true],'pause: a finished switch (console) still translates the list again');
 // the rows are built from the kinds that have choices, so the two rows are consecutive: K33_ROW * (i + 1) under the anchor
 assert(text.includes('made.y = anchor.y + K33_ROW * (i + 1);')&&text.includes('row.y += K33_ROW * kinds.length;')&&text.includes('bg.height += K33_ROW * kinds.length;'),'pause: rows close up, the list grows by the rows it has');
 assert(/if\(K33_STYLE_LANGUAGE_ROW\)\s*\{\s*\/\/ the language line[\s\S]*?this\.k33StyleNote = tf;/.test(text),'pause: the language line is made only with the Language row');
}

// --- Options > Graphics: drop-downs. The tab's actual build / show / change / frame code against combo boxes that behave
// as C1's CodoComboBox does: setting the list selects its first entry, and every selection fires EVT_CHANGE ---------
{
 const text=SOURCES.video;
 global.K33_GLYPH_NAMES=literal(text,'K33_GLYPH_NAMES');global.K33_STYLE_DVARS=literal(text,'K33_STYLE_DVARS');
 global.K33_STYLE_KINDS=literal(text,'K33_STYLE_KINDS');global.K33_STYLE_LABELS=literal(text,'K33_STYLE_LABELS');
 global.K33_STYLE_POLL=Number(/K33_STYLE_POLL:int = (\d+)/.exec(text)[1]);global.K33_STYLE_HOLD=Number(/K33_STYLE_HOLD:int = (\d+)/.exec(text)[1]);
 for(const n of ['K33_STYLE_X','K33_STYLE_Y','K33_STYLE_PITCH','K33_STYLE_Y3','K33_STYLE_PITCH3','K33_STYLE_NOTE_Y'])
  global[n]=Number(new RegExp(n+':Number = ([0-9.]+);').exec(text)[1]);
 function tab(){
  const v=logic('video',['k33StyleOf','k33StyleShow','k33OnStyleChange','k33OnStyleOpen','k33OnStyleClose','k33StyleRemove','k33StyleBuild','k33StyleNoteUpdate','k33StyleFrame']);
  Object.assign(v,{stage:{},_k33StyleRows:[],_k33StyleCombos:{},_k33StyleLabels:{},_k33StyleOpen:null,_k33StyleSync:false,_k33StyleDirty:false,
   _k33StyleNote:null,_k33StyleNoteKey:'',_k33StyleSerial:0,_k33StyleFrames:0,relocalized:0,made:[],
   myTFSSAO:{y:249.9},myCBSSAO:{y:244},myCBDisplayMode:{x:98},children:[],
   addChild(o){o.parent=this;this.children.push(o);},removeChild(o){o.parent=null;this.children.splice(this.children.indexOf(o),1);},
   k33StyleText(src,str,size,color){return {text:str,x:0,y:0,visible:true,parent:null};},
   k33StyleLabel(kind){if(!this._k33StyleLabels[kind])this._k33StyleLabels[kind]={text:'',x:K33_STYLE_X,y:0,visible:false};return this._k33StyleLabels[kind];},
   k33StyleCombo(kind){                                // CodoComboBox, as far as the rows use it
    if(!this._k33StyleCombos[kind]){
     const tab=this,cb={kind,x:98,y:0,visible:false,changes:0,closed:0,_dp:[],_i:-1,
      get dataProvider(){return this._dp;},
      set dataProvider(dp){const old=this._dp[this._i];this._dp=dp;if(dp.length&&(old===undefined||dp[this._i]!==old))this.selectedDataIndex=0;},
      get selectedDataIndex(){return this._i;},
      set selectedDataIndex(i){this._i=i;if(i>=0){this.changes++;tab.k33OnStyleChange({currentTarget:this});}},
      get shown(){return this._i<0?'':this._dp[this._i].label;},
      pick(i){this.selectedDataIndex=i;},                // the player picks a row of the open list
      close(){this.closed++;tab.k33OnStyleClose({currentTarget:this});}};
     this._k33StyleCombos[kind]=cb;this.made.push(kind);
    }
    return this._k33StyleCombos[kind];},
   k33StyleRelocalize(){this.relocalized++;this.k33StyleBuild();}});
  v.frames=n=>{for(let i=0;i<n;i++)v.k33StyleFrame(null);};
  v.cb=kind=>v._k33StyleCombos[kind];
  v.rows=()=>v._k33StyleRows.map(r=>[r.kind,r.label.text,r.combo.shown,r.combo.y]);
  return v;
 }

 // what ships: the Language row off -> two rows, no gap, the dvars' values shown by name, nothing set
 global.K33_STYLE_LANGUAGE_ROW=false;
 strings_extra=()=>Object.assign(strings,{K33UI_LANGUAGE:'Language',K33UI_GLYPHS:'Button icons',K33UI_HUD_STYLE:'HUD style',K33UI_LANG_RESTART:'Restart the game to apply the language',K33UI_LANG_SWITCHING:'Switching language...'});
 engine({k33_language:'zh',gpad_glyphs:'2',k33_hudStyle:'2',k33_languageState:'2'});strings_extra();
 let v=tab();v.k33StyleBuild();
 assert.deepStrictEqual(v.rows(),[['glyph','Button icons','PlayStation',K33_STYLE_Y],['hud','HUD style','Version 2.6',K33_STYLE_Y+K33_STYLE_PITCH]],'two drop-downs under each other, labels and names translated');
 assert.deepStrictEqual(v.made,['glyph','hud'],'no Language box is made');assert.strictEqual(v._k33StyleNote,null,'no language line is made');
 assert.deepStrictEqual(sets,[],'filling and selecting from the dvars sets nothing');
 assert.deepStrictEqual(reads.filter(n=>n.indexOf('k33_language')===0),[],'no k33_language* dvar is read to make the rows');
 assert.deepStrictEqual(v.cb('glyph').dataProvider.map(d=>d.id),['0','1','2','3','4','5','6']);
 assert(v.cb('glyph').dataProvider.every(d=>typeof d.label==='string'&&d.label[0]!=='@'&&!('name' in d)),'list entries are {label, id} with translated labels');
 assert(v.rows().every(r=>r[1][0]!=='@'),'a label is never an untranslated @KEY');
 assert.strictEqual(v._k33StyleLabels.glyph.y,K33_STYLE_Y+(249.9-244),'the label sits by its box as the Graphics rows\' labels do');
 // the player picks: shown at once (the box does that), set at once, once
 v.cb('hud').pick(0);assert.deepStrictEqual(sets,[['k33_hudStyle','0']]);assert.strictEqual(v.k33StyleOf('hud').shown,0);assert.strictEqual(v.k33StyleOf('hud').hold,K33_STYLE_HOLD);
 v.cb('hud').pick(0);assert.strictEqual(sets.length,1,'picking what the dvar holds sets nothing');
 v.cb('glyph').pick(6);assert.deepStrictEqual(sets[1],['gpad_glyphs','6']);assert.strictEqual(dvars.gpad_glyphs,'6');
 // Apply / Cancel / Default are not involved: the dvars already hold the picks
 // a refused value (old DLL: gpad_glyphs 0..5) stays on show only for the hold, then the box shows the dvar again, setting nothing
 ranges.gpad_glyphs=[0,5];dvars.gpad_glyphs='2';v.cb('glyph').pick(2);sets=[];v.cb('glyph').pick(6);
 assert.deepStrictEqual(sets,[['gpad_glyphs','6']]);assert.strictEqual(v.cb('glyph').shown,'Keyboard & mouse');
 v.frames(K33_STYLE_POLL);assert.strictEqual(v.cb('glyph').shown,'Keyboard & mouse','held while the engine takes the set');
 v.frames(K33_STYLE_HOLD+K33_STYLE_POLL);assert.strictEqual(v.cb('glyph').shown,'PlayStation','read back after the hold');
 assert.deepStrictEqual(sets,[['gpad_glyphs','6']],'the read-back selects without setting');
 // a value changed elsewhere (console, the pause menu) is followed, without a set
 sets=[];dvars.k33_hudStyle='2';v.frames(K33_STYLE_POLL);assert.strictEqual(v.cb('hud').shown,'Version 2.6');assert.deepStrictEqual(sets,[]);
 // an open list is left alone until it closes
 v.k33OnStyleOpen({currentTarget:v.cb('hud')});dvars.k33_hudStyle='0';v.frames(K33_STYLE_POLL*2);assert.strictEqual(v.cb('hud').shown,'Version 2.6','not changed under the player\'s cursor');
 v.k33OnStyleClose({currentTarget:v.cb('hud')});v.frames(K33_STYLE_POLL);assert.strictEqual(v.cb('hud').shown,'Classic (1.1.1.13)');
 // a value the list does not have shows the first entry and writes nothing
 engine({k33_hudStyle:'5',gpad_glyphs:'9'});strings_extra();v=tab();v.k33StyleBuild();
 assert.deepStrictEqual(v.rows().map(r=>r[2]),['Auto','Classic (1.1.1.13)']);v.frames(K33_STYLE_POLL*3);assert.deepStrictEqual(sets,[],'unknown value: first entry, nothing written');
 assert.strictEqual(dvars.k33_hudStyle,'5');
 // a list that is empty hides its row and the other closes up; when the list turns up the row is made
 engine({k33_padinfo:''});strings_extra();v=tab();v.k33StyleBuild();
 assert.deepStrictEqual(v.rows(),[['hud','HUD style','Classic (1.1.1.13)',K33_STYLE_Y]],'HUD style moves up when Button icons has no list');
 dvars.k33_padinfo='0|1|pad';v.frames(K33_STYLE_POLL);assert.deepStrictEqual(v.rows().map(r=>[r[0],r[3]]),[['glyph',K33_STYLE_Y],['hud',K33_STYLE_Y+K33_STYLE_PITCH]],'the row appears when its list does');
 dvars.k33_hudStyleList='';v.frames(K33_STYLE_POLL);assert.deepStrictEqual(v.rows().map(r=>r[0]),['glyph']);
 assert.strictEqual(v.cb('hud').visible,false);assert.strictEqual(v._k33StyleLabels.hud.visible,false,'a row without a list is hidden, box and label');
 engine({k33_padinfo:null,k33_hudStyleList:null});v=tab();v.k33StyleBuild();assert.deepStrictEqual(v.rows(),[],'an older Kernel33.dll: no rows');assert.deepStrictEqual(v.made,[]);
 // a rebuild closes an open list first
 engine();strings_extra();v=tab();v.k33StyleBuild();v.k33OnStyleOpen({currentTarget:v.cb('glyph')});v.k33StyleBuild();assert.strictEqual(v.cb('glyph').closed,1);assert.strictEqual(v._k33StyleOpen,null);
 // a finished language switch (k33_languageSerial, e.g. k33_setlanguage in the console): labels and list names are set again
 engine({gpad_glyphs:'5',k33_hudStyle:'2'});strings_extra();v=tab();v.k33StyleBuild();const boxes=[v.cb('glyph'),v.cb('hud')];
 Object.assign(strings,{K33UI_GLYPHS:'按键图标',K33UI_HUD_STYLE:'HUD 样式',K33UI_GLYPHS_TEXT:'仅文字',K33UI_HUD_STYLE_V26:'2.6 版'});
 v.frames(K33_STYLE_POLL*2);assert.strictEqual(v.relocalized,0,'not before the serial moves');
 dvars.k33_languageSerial='3';v.frames(K33_STYLE_POLL*2);assert.strictEqual(v.relocalized,1,'a finished switch translates the tab again, once');
 assert.deepStrictEqual(v.rows().map(r=>[r[1],r[2]]),[['按键图标','仅文字'],['HUD 样式','2.6 版']],'row labels and the boxes\' entries in the new language, the selection kept');
 assert.deepStrictEqual([v.cb('glyph'),v.cb('hud')],boxes,'the same boxes are used again');assert.deepStrictEqual(sets,[],'re-localising sets nothing');
 // not on the stage yet: the rows are made by the first frame
 engine();strings_extra();v=tab();v.stage=null;v.k33StyleBuild();assert.deepStrictEqual(v.rows(),[]);assert.strictEqual(v._k33StyleDirty,true);
 v.stage={};v.frames(1);assert.strictEqual(v.rows().length,2);

 // the Language row switched on: three rows on the tighter pitch, the language line, a pick sets k33_language
 global.K33_STYLE_LANGUAGE_ROW=true;
 engine({k33_language:'zh',k33_languageState:'2'});strings_extra();v=tab();v.k33StyleBuild();
 assert.deepStrictEqual(v.rows().map(r=>[r[0],r[2],r[3]]),[['lang','中文',K33_STYLE_Y3],['glyph','Auto',K33_STYLE_Y3+K33_STYLE_PITCH3],['hud','Classic (1.1.1.13)',K33_STYLE_Y3+2*K33_STYLE_PITCH3]]);
 assert(K33_STYLE_Y3+2*K33_STYLE_PITCH3+24<=467,'three rows end above the warning line');
 assert.strictEqual(v._k33StyleNote.text,'Restart the game to apply the language','state 2: the restart line, translated before it is set');assert.strictEqual(v._k33StyleNote.visible,true);
 dvars.k33_languageState='1';v.frames(K33_STYLE_POLL);assert.strictEqual(v._k33StyleNote.text,'Switching language...');
 dvars.k33_languageState='0';v.frames(K33_STYLE_POLL);assert.strictEqual(v._k33StyleNote.visible,false);
 v.cb('lang').pick(0);assert.deepStrictEqual(sets,[['k33_language','en']]);
 engine({k33_language:'fr'});strings_extra();v=tab();v.k33StyleBuild();assert.strictEqual(v.cb('lang').shown,'English');v.frames(K33_STYLE_POLL*2);assert.deepStrictEqual(sets,[],'unknown language: first entry, nothing written');
 dvars.k33_languageList='en';v.frames(K33_STYLE_POLL);assert.deepStrictEqual(v.cb('lang').dataProvider.map(d=>d.id),['en'],'a changed list makes the rows again');
 global.K33_STYLE_LANGUAGE_ROW=false;
}

// --- wiring ---------------------------------------------------------------------------------------------------
const P=SOURCES.pause,V=SOURCES.video;
assert(P.includes('this.addEventListener(Event.ENTER_FRAME,this.k33StyleFrame); // k33: style rows'),'pause: the rows follow their dvars every frame');
assert(P.includes('this.k33AddStyleRows(view); // k33: style rows, under the FOV row'),'pause: rows added with the FOV row');
assert(P.includes('row != this.k33FovRow && this.k33StyleOf(row) == null'),"pause: C1's row list leaves the style rows out");
assert(P.includes('view.gotoMenu(view.getCurrentMenu());'),'pause: a language switch builds the open list again');
assert(/if\(code != 244 \|\| this\.k33StyleOf\(cur\) != null\)/.test(P),'pause: pad select steps a style row');
assert(V.includes('this.k33StyleWatch(true);')&&V.includes('this.k33StyleWatch(false); // k33'),'video: watched only while the tab is shown');
// video: the rows are the tab's own combo box class, made in code; they open upwards, come to the front as they open
// (so the list is not behind the check boxes) and need no pad hint: PadSnap finds a combo box's button and list rows
assert(V.includes('cls = getDefinitionByName(getQualifiedClassName(this.myCBAspectRatio)) as Class;')&&V.includes('cb = new cls() as CodoComboBox;'),"video: another instance of the tab's own combo box class");
assert(V.includes('cb.setDropDownAtRuntime(false);'),'video: the list opens upwards (the rows are at the foot of the panel)');
assert(V.includes('cb.addEventListener(CodoComboBox.EVT_PRE_OPEN,this.onK33ComboPreOpen);')&&V.includes('this.setChildIndex(tempCB,this.numChildren - 1);'),'video: an opening list comes to the front');
assert(V.includes('cb.addEventListener(CodoComboBox.EVT_CHANGE,this.k33OnStyleChange);'),'video: a pick applies at once');
assert(V.includes('cb.x = this.myCBDisplayMode.x;'),'video: the boxes are in the Display Mode column');
assert(!/PadSnap|k33StyleButton|k33OnStyleClick|drawRect/.test(V),'video: no code-drawn stepper is left');
// a code-made text field never gets "@KEY" (C1's translator would answer in the default format: black boxes)
assert(V.includes('lab.text = LocalizeUtil.localize(K33_STYLE_LABELS[kinds[i]]);'),'video: row labels are translated before they are set');
for(const [n,t] of Object.entries(SOURCES)){
 assert(!/\.text = key;/.test(t),n+': the language line is translated before it is set');
 assert(/Note\.text = key == "" \? "" : LocalizeUtil\.localize\(key\);/.test(t),n+': the language line goes through LocalizeUtil.localize');
}
assert(!/k33StyleText\([^;]*"@/.test(V)&&!/k33StyleText\([^;]*K33_STYLE_LABELS/.test(V),'video: no "@KEY" is handed to a code-made field');
assert(V.includes('this.k33Texts(); // k33: the texts, set again after a language switch'),'video: onInit texts are re-settable');
for(const [n,t] of Object.entries(SOURCES)){
 assert(!/Dvar_Set|setDvar\(|execnow\(/.test(shared(t)),n+': sets go through CFunctionContainerCommon.setdvar');
 for(const d of ['k33_languageList','k33_hudStyleList','k33_padinfo','k33_languageState','k33_languageSerial'])assert(t.includes('"'+d+'"'),n+' reads '+d);
}
// Apply / Cancel / Default of the Graphics tab do not touch the rows' dvars
for(const fn of ['onApplyBtnClickHandle','onDefaultBtnClickHandle','onCancelBtnClickHandle','isVideoChanged']){
 const at=V.indexOf('function '+fn+'(');const end=V.indexOf('\n      }',at);
 assert(!/k33Style|k33_language|gpad_glyphs|k33_hudStyle/.test(V.slice(at,end)),fn+' stays clear of the style rows');
}

// --- every key the rows show has an English and a Chinese entry, once per language pack --------------------------
function entries(zone){
 const out={};
 for(const line of read('zones/'+zone+'/localize.str').split('\n')){if(!line||line[0]==='#')continue;const t=line.indexOf('\t');out[line.slice(0,t)]=line.slice(t+1);}
 return out;
}
const used=new Set();
for(const t of Object.values(SOURCES))for(const m of t.matchAll(/"@(K33UI_(?:LANGUAGE|GLYPHS|HUD_STYLE|LANG_[A-Z]+))"/g))used.add(m[1]);
for(const n of literal(P,'K33_GLYPH_NAMES'))used.add('K33UI_GLYPHS_'+n);
for(const id of ['EN','ZH'])used.add('K33UI_LANG_NAME_'+id);
for(const id of ['STOCK','V12','V26','V33','V35','SM2'])used.add('K33UI_HUD_STYLE_'+id);
assert.strictEqual(used.size,20,[...used].join(' '));
const en=entries('codol_english_ui'),zh=entries('codol_chinese_ui'),enx=entries('codol_english_ui_extra'),zhx=entries('codol_chinese_ui_extra');
const keysPy=read('scripts/ui_keys.py');
for(const lang of ['english','chinese']){
 const manifest=read('zone_source/codol_'+lang+'_ui.csv').split('\n');
 for(const k of used)assert.strictEqual(manifest.filter(l=>l==='localize,'+k).length,1,k+' in zone_source/codol_'+lang+'_ui.csv');
 const pack=read('zone_packs/codol_'+lang+'_locale.csv');
 assert(pack.includes('part,codol_'+lang+'_ui\n')&&pack.includes('part,codol_'+lang+'_ui_extra\n'),'both string zones load as one pack, so the pause menu sees the keys');
}
for(const k of used){
 assert(en[k]&&zh[k],k+' needs an English and a Chinese entry');
 assert(!(k in enx)&&!(k in zhx),k+' must not be entered twice in one language pack');
 assert(keysPy.includes('"'+k+'": ("'+en[k]+'", "'+zh[k]+'", "codo.gfx")'),k+' in scripts/ui_keys.py NAMED with the same texts');
}
assert.strictEqual(en.K33UI_LANG_NAME_EN,'English');assert.strictEqual(zh.K33UI_LANG_NAME_EN,'English');
assert(en.K33UI_LANG_NAME_ZH.startsWith('中文')&&zh.K33UI_LANG_NAME_ZH==='中文','each language in its own name');
console.log('UI style rows: shared logic identical in both movies; Language row on, and with the switch off no row, no gap, no dvar read; Graphics drop-downs (fill, pick, hold, read-back, unlisted values, empty lists, re-localise); '+used.size+' localize keys passed');
