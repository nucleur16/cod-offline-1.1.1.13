// The Language / Button icons / HUD style rows (docs/UI_STYLE_OPTIONS_20261112.md): runs the rows' actual
// ActionScript logic from both movies' sources -- codoingame.gfx MainPause (pause menu) and codo.gfx OptionView_video
// (Options > Graphics) -- against a fake dvar table, and checks the wiring and the localize keys they rely on.
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
let dvars,ranges,sets,strings;
global.int=v=>Math.trunc(Number(v))||0;
global.CFunctionContainerCommon={
 Dvar_GetString:n=>dvars[n]===undefined?'':String(dvars[n]),
 Dvar_GetInt:n=>dvars[n]===undefined?0:(parseInt(dvars[n],10)||0),
 setdvar:(n,v)=>{sets.push([n,v]);if(dvars[n]===undefined)return;            // a dvar the DLL does not have: nothing
  if(ranges[n]&&(Number(v)<ranges[n][0]||Number(v)>ranges[n][1]))return;      // out of range: the engine refuses it
  dvars[n]=v;},
};
global.LocalizeUtil={localize:k=>strings[k.slice(1)]===undefined?k:strings[k.slice(1)]};
function engine(over){
 dvars=Object.assign({k33_language:'en',k33_languageList:'en;zh',k33_languageState:'0',k33_languageSerial:'0',
  gpad_glyphs:'0',k33_padinfo:'0|1|Xbox 360 Controller|0|0',k33_glyphSerial:'0',
  k33_hudStyle:'0',k33_hudStyleList:'0:stock;2:v26'},over||{});
 for(const k of Object.keys(dvars))if(dvars[k]===null)delete dvars[k];
 ranges={gpad_glyphs:[0,6],k33_hudStyle:[0,9]};sets=[];
 strings={K33UI_LANG_NAME_EN:'English',K33UI_LANG_NAME_ZH:'中文',K33UI_HUD_STYLE_STOCK:'Classic (1.1.1.13)',K33UI_HUD_STYLE_V26:'Version 2.6',
  K33UI_GLYPHS_AUTO:'Auto',K33UI_GLYPHS_XBOX:'Xbox',K33UI_GLYPHS_PS:'PlayStation',K33UI_GLYPHS_NINTENDO:'Wii U / Switch',
  K33UI_GLYPHS_GENERIC:'Generic',K33UI_GLYPHS_TEXT:'Text only',K33UI_GLYPHS_KBM:'Keyboard & mouse'};
}
function logic(which,extra){
 const text=SOURCES[which],o={};
 for(const n of SHARED.concat(extra||[]))o[n]=body(text,n);
 return o;
}

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

// --- Options > Graphics: the same follow-the-dvar loop ---------------------------------------------------------
{
 const text=SOURCES.video;
 global.K33_GLYPH_NAMES=literal(text,'K33_GLYPH_NAMES');global.K33_STYLE_DVARS=literal(text,'K33_STYLE_DVARS');
 global.K33_STYLE_KINDS=literal(text,'K33_STYLE_KINDS');
 global.K33_STYLE_POLL=Number(/K33_STYLE_POLL:int = (\d+)/.exec(text)[1]);global.K33_STYLE_HOLD=Number(/K33_STYLE_HOLD:int = (\d+)/.exec(text)[1]);
 engine({k33_hudStyle:'2'});
 const v=logic('video',['k33StyleOf','k33StyleFrame']);
 Object.assign(v,{_k33StyleRows:[],_k33StyleSerial:0,_k33StyleFrames:0,builds:0,relocalized:0,
  k33StyleShow(rec,i){rec.shown=i;},k33StyleBuild(){this.builds++;},k33StyleRelocalize(){this.relocalized++;},k33StyleNoteUpdate(){}});
 for(const kind of K33_STYLE_KINDS){const opts=v.k33StyleOptions(kind);v._k33StyleRows.push({kind,opts,shown:v.k33StyleIndex(opts,v.k33StyleCurrent(kind)),hold:0});}
 const frames=n=>{for(let i=0;i<n;i++)v.k33StyleFrame(null);};
 assert.strictEqual(v.k33StyleOf('hud').shown,1);
 dvars.k33_hudStyle='0';frames(K33_STYLE_POLL);assert.strictEqual(v.k33StyleOf('hud').shown,0,'follows the dvar');
 v.k33StyleOf('glyph').shown=6;v.k33StyleOf('glyph').hold=K33_STYLE_HOLD;frames(K33_STYLE_POLL);assert.strictEqual(v.k33StyleOf('glyph').shown,6,'held');
 frames(K33_STYLE_HOLD+K33_STYLE_POLL);assert.strictEqual(v.k33StyleOf('glyph').shown,0,'read back after the hold');
 assert.strictEqual(v.builds+v.relocalized,0);
 dvars.k33_languageSerial='3';frames(K33_STYLE_POLL*2);assert.strictEqual(v.relocalized,1,'a finished switch translates the tab again, once');
 dvars.k33_languageList='en';frames(K33_STYLE_POLL);assert.strictEqual(v.builds,1,'a changed list makes the rows again');
}

// --- wiring ---------------------------------------------------------------------------------------------------
const P=SOURCES.pause,V=SOURCES.video;
assert(P.includes('this.addEventListener(Event.ENTER_FRAME,this.k33StyleFrame); // k33: style rows'),'pause: the rows follow their dvars every frame');
assert(P.includes('this.k33AddStyleRows(view); // k33: style rows, under the FOV row'),'pause: rows added with the FOV row');
assert(P.includes('row != this.k33FovRow && this.k33StyleOf(row) == null'),"pause: C1's row list leaves the style rows out");
assert(P.includes('view.gotoMenu(view.getCurrentMenu());'),'pause: a language switch builds the open list again');
assert(/if\(code != 244 \|\| this\.k33StyleOf\(cur\) != null\)/.test(P),'pause: pad select steps a style row');
assert(V.includes('this.k33StyleWatch(true);')&&V.includes('this.k33StyleWatch(false); // k33'),'video: watched only while the tab is shown');
assert(V.includes('PadSnap.hint(btn,PadSnap.SPLIT_H);'),'video: the pad cursor stops on each half of a stepper');
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
for(const id of ['STOCK','V12','V26','V33','V35'])used.add('K33UI_HUD_STYLE_'+id);
assert.strictEqual(used.size,19,[...used].join(' '));
const en=entries('codol_english_ui'),zh=entries('codol_chinese_ui'),enx=entries('codol_english_ui_extra'),zhx=entries('codol_chinese_ui_extra');
const keysPy=read('ui_keys.py');
for(const lang of ['english','chinese']){
 const manifest=read('zone_source/codol_'+lang+'_ui.csv').split('\n');
 for(const k of used)assert.strictEqual(manifest.filter(l=>l==='localize,'+k).length,1,k+' in zone_source/codol_'+lang+'_ui.csv');
 const pack=read('zone_packs/codol_'+lang+'_locale.csv');
 assert(pack.includes('part,codol_'+lang+'_ui\n')&&pack.includes('part,codol_'+lang+'_ui_extra\n'),'both string zones load as one pack, so the pause menu sees the keys');
}
for(const k of used){
 assert(en[k]&&zh[k],k+' needs an English and a Chinese entry');
 assert(!(k in enx)&&!(k in zhx),k+' must not be entered twice in one language pack');
 assert(keysPy.includes('"'+k+'": ("'+en[k]+'", "'+zh[k]+'", "codo.gfx")'),k+' in ui_keys.py NAMED with the same texts');
}
assert.strictEqual(en.K33UI_LANG_NAME_EN,'English');assert.strictEqual(zh.K33UI_LANG_NAME_EN,'English');
assert(en.K33UI_LANG_NAME_ZH.startsWith('中文')&&zh.K33UI_LANG_NAME_ZH==='中文','each language in its own name');
console.log('UI style rows: shared logic identical in both movies; list parsing, missing dvars, unlisted values, wraparound, refused sets, language serial and '+used.size+' localize keys passed');
