// Run actual kiosk transaction bodies. Native UI/loadout application are boundaries.
const fs=require('fs'),path=require('path'),assert=require('assert');
const raw=path.join(__dirname,'../bin/home/codol_rawfiles');
const k=fs.readFileSync(path.join(raw,'maps/mp/gametypes/_survival_kiosk.gsc'),'utf8');
const s=fs.readFileSync(path.join(raw,'maps/mp/gametypes/_survival.gsc'),'utf8');
const cl=fs.readFileSync(path.join(raw,'maps/mp/gametypes/_class.gsc'),'utf8');
function body(src,n){let a=src.indexOf('\n'+n+'(');assert(a>=0,n);a=src.indexOf('{',a);let b=a+1,d=1;while(d){if(src[b]==='{')d++;if(src[b]==='}')d--;b++;}return src.slice(a+1,b-1);}
function lower(x){return x.replace(/\bself\b/g,'this').replace(/\.size/g,'.length').replace(/this maps\\mp\\gametypes\\_survival::(\w+)\(/g,'this.$1(').replace(/maps\\mp\\gametypes\\_class::hasLoadoutClass/g,'hasClass').replace(/this \[\[level.survival_kiosk_class_handler\]\]/g,'this.originalClass').replace(/this (\w+)\(/g,'this.$1(').replace('this thread maps\\mp\\gametypes\\_survival::after_class_change();','this.after_class_change();');}
global.isDefined=v=>v!==undefined&&v!==null;global.isAlive=p=>p.health>0;global.getSubStr=(s,a,b)=>s.slice(a,b);
global.game={state:'playing',menu_changeclass:'changeclass',menu_changeclass_allies:'changeclass_marines',menu_changeclass_axis:'changeclass_opfor'};global.level={survival_over:false,classMap:{custom1:0,custom2:1,custom8:7,custom9:8,class0:0}};
global.hasClass=new Function('classIndex',body(cl,'codol_hasLoadoutClass'));
function player(balance){return {health:100,sessionstate:'playing',survival_credit:balance,pers:{class:'custom1',team:'allies'},applied:0,menu:[],frozen:[],notifies:[],survival_bought:['oldgun'],
 class_purchase_live:new Function(lower(body(k,'class_purchase_live'))),
 cancel_class_purchase:new Function(lower(body(k,'cancel_class_purchase'))),
 response:new Function('response','var cost;'+lower(body(k,'class_menu_response'))),
 spend_credits:new Function('value',lower(body(s,'spend_credits'))),
 add_credits(value){this.survival_credit+=value;},setClientDvar(n,v){this[n]=v;},closeInGameMenu(){this.menu.push('closePause');},closeMenu(){this.menu.push('close');},openpopupMenu(n){this.menu.push(n);},openMenu(n){this.menu.push(n);},freezeControls(v){this.frozen.push(v);},notify(n){this.notifies.push(n);},iPrintLnBold(){},
 after_class_change(){this.restored=true;},originalClass(response){if(this.pers.class!==response){assert(this.survival_class_change,'native immediate-apply permission');this.pers.class=response;this.applied++;}}};}
let p=player(2000);p.survival_class_purchase_cost=1000;p.response('custom2');assert.equal(p.survival_credit,1000);assert.equal(p.applied,1);assert.equal(p.pers.class,'custom2');assert.equal(p.survival_class_change,false);assert.deepEqual(p.survival_bought,[]);assert.equal(p.survival_class_purchase_cost,undefined);
p.response('back');assert.equal(p.survival_class_purchase,0);p.response('custom2');assert.equal(p.survival_credit,1000);assert.equal(p.applied,1,'duplicate reply never double-debits/reapplies');
for(const response of ['custom1','class0','custom9','not_a_class','',undefined]){p=player(2000);p.survival_class_purchase_cost=1000;p.response(response);assert.equal(p.survival_credit,2000);assert.equal(p.applied,0);assert.equal(p.pers.class,'custom1');}
p=player(999);p.survival_class_purchase_cost=1000;p.response('custom2');assert.equal(p.survival_credit,999);assert.equal(p.pers.class,'custom1');assert.equal(p.applied,0,'wallet revalidated at selection, not merely at open');
for(const change of [p=>p.health=0,p=>p.sessionstate='spectator',p=>p.laststand=true,p=>level.survival_over=true,p=>game.state='postgame']){p=player(2000);p.survival_class_purchase_cost=1000;change(p);p.response('custom2');assert.equal(p.survival_credit,2000);assert.equal(p.applied,0);level.survival_over=false;game.state='playing';}
// Actual class-menu open body yields until the kiosk handler returned. It
// opens C1's existing menu; no fabricated class list and no debit at open.
const Gen=Object.getPrototypeOf(function*(){}).constructor;
let open=lower(body(k,'choose_class_menu')).replace(/this\.endon\([^;]+;/g,'').replace('waittillframeend;','yield "frame";').replace(/this thread (\w+)\(/g,'this.startThread("$1",');
const choose=new Gen(open);p=player(2000);p.survival_class_purchase_cost=1000;p.close_menu=function(){this.closeMenu();};p.startThread=function(){};
let run=choose.call(p);assert.equal(run.next().value,'frame');assert(run.next().done);assert.equal(p.survival_credit,2000);assert(p.menu.includes('changeclass_marines'));assert(!p.menu.includes('changeclass'));assert.equal(p.survival_class_purchase,1);assert.equal(p.frozen.at(-1),true);
let cancel=lower(body(k,'class_purchase_cancel_response')).replace(/this\.endon\([^;]+;/g,'').replace('this.waittill( "menuresponse", menu, response );','({menu,response}=yield "menuresponse");');
const cancelResponse=new Gen('var menu,response;'+cancel);
for(const response of ['back','quit']){p=player(2000);p.survival_class_purchase_cost=1000;run=cancelResponse.call(p);run.next();assert(run.next({menu:'changeclass',response}).done);assert.equal(p.survival_credit,2000);assert.equal(p.survival_class_purchase_cost,undefined);assert.equal(p.frozen.at(-1),false);}
let interrupt=lower(body(k,'class_purchase_interrupt')).replace(/this\.endon\([^;]+;/g,'').replace('this.waittill( event );','yield event;');const stop=new Gen('event',interrupt);
for(const event of ['death','disconnect','player_downed']){p=player(2000);p.survival_class_purchase_cost=1000;run=stop.call(p,event);assert.equal(run.next().value,event);run.next();assert.equal(p.survival_credit,2000);assert.equal(p.survival_class_purchase_cost,undefined);}
assert(!/\bwait\b/.test(body(k,'class_menu_response').replace(/\/\/[^\n]*/g,'')),'commit has no scheduling gap');
assert(body(k,'init').includes('level.survival_kiosk_class_handler = level.class;'));
assert(body(k,'purchase').indexOf('ref == "locker"')<body(k,'purchase').indexOf('spend_credits( cost )'));
console.log('Actual kiosk transaction: real custom menu opens free, valid profile selection charges/applies once; Escape/back/quit, death/down/disconnect, unchanged/invalid class and insufficient selection balance are free; duplicate reply safe');

// Exercise the real _menus callback rather than only its transaction boundary.
const menuSrc=fs.readFileSync(path.join(raw,'maps/mp/gametypes/_menus.gsc'),'utf8');
let nativeBody=body(menuSrc,'menuClass').replace(/\bclass\b/g,'classChoice').replace(/"classChoice"/g,'"class"');
nativeBody=lower(nativeBody).replace(/this maps\\mp\\gametypes\\_class::(\w+)\(/g,'this.$1(')
 .replace('this [[level.survivalClassChange]]()','this.immediate()')
 .replace('this thread maps\\mp\\gametypes\\_spectating::setSpectatePermissions();','this.spectateUpdated=true;').replace('this thread maps\\mp\\gametypes\\_playerlogic::spawnClient();','this.spawnCalled=true;');
const actualMenuClass=new Function('response','var classChoice,primary;'+nativeBody);
level.survivalClassChange=true;level.inGracePeriod=false;level.partyMode=false;
p=player(2000);p.survival_class_purchase_cost=1000;p.pers.primary=0;p.hasDoneCombat=true;
p.closeMenus=function(){};p.getClassChoice=response=>response;p.getWeaponChoice=()=>0;
p.setClass=function(c){this.classApplied=c;};p.giveLoadout=function(team,c){this.loadoutApplied=[team,c];};
p.immediate=function(){if(this.survival_class_change){this.survival_class_change=false;this.after_class_change();return true;}return false;};
p.originalClass=actualMenuClass;p.response('custom2');assert.equal(p.pers.class,'custom2');assert.equal(p.classApplied,'custom2');
assert.deepEqual(p.loadoutApplied,['allies','custom2']);assert.equal(p.survival_credit,1000);assert(p.restored);assert(p.spectateUpdated);
console.log('Actual _menus class callback applies the purchased custom loadout in the same transaction outside grace period');
