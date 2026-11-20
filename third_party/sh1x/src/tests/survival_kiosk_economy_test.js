// Execute the actual GSC credit and purchase functions; stub only engine/equipment boundaries.
const fs=require('fs'),path=require('path'),assert=require('assert');
const root=path.join(__dirname,'../bin/home/codol_rawfiles');
const survival=fs.readFileSync(path.join(root,'maps/mp/gametypes/_survival.gsc'),'utf8');
const kiosk=fs.readFileSync(path.join(root,'maps/mp/gametypes/_survival_kiosk.gsc'),'utf8');
function body(source,name) {
  const start=source.indexOf('\n'+name+'('),open=source.indexOf('{',start);assert(start>=0,name);
  let depth=1,end=open+1;while(depth){if(source[end]==='{')depth++;if(source[end]==='}')depth--;end++;}
  return source.slice(open+1,end-1).replace(/self/g,'this').replace(/\.size/g,'.length')
    .replace(/this maps\\mp\\gametypes\\_survival::(\w+)\s*\(/g,'this.$1(')
    .replace(/this\.survival_hud_credits setValue\s*\(/g,'this.survival_hud_credits.setValue(')
    .replace(/this (\w+)\s*\(/g,'this.$1(');
}
let data={};
// Tables contain quoted text but the rows/refs/cost columns exercised here never contain a comma.
for(const line of fs.readFileSync(path.join(root,'mp/kiosks.csv'),'utf8').split(/\r?\n/)){
 const row=line.split(',');if(/^\d+$/.test(row[0]))data[row[0]]=row;
}
global.isDefined=v=>v!==undefined&&v!==null;global.int=v=>Math.trunc(Number(v)||0);
global.tableLookup=(table,key,value,column)=>(data[value]||[])[column]||'';
global.level={survival_kiosk_table:'mp/kiosks.csv',survival_max_perks:3};
global.olprintln=()=>{};
global.item_ref=new Function('index',body(kiosk,'item_ref'));
global.item_tier=new Function('index',body(kiosk,'item_tier'));
global.item_cost=new Function('index','var ref,i,tier;'+body(kiosk,'item_cost'));
global.menu_rows=kind=>kind==='kiosk_locker'?[0,1,2,3,4]:[10,11,13,14];
function player(balance){
 const p={survival_credit:balance,score:balance,name:'p',clientDvars:{},updates:0,grants:0,allow:true,grant:true,
  survival_hud_credits:{value:balance,setValue(v){this.value=v;}},
  notify(){this.updates++;},setClientDvar(n,v){this.clientDvars[n]=v;},
  setClientDvars(...pairs){for(let i=0;i<pairs.length;i+=2)this.setClientDvar(pairs[i],pairs[i+1]);},
  iPrintLnBold(){},can_buy(){return this.allow;},give_item(){if(!this.grant)return false;this.grants++;return true;}};
 p.add_credits=new Function('value,why',body(survival,'add_credits'));
 p.spend_credits=new Function('value',body(survival,'spend_credits'));
 p.purchase=new Function('index','var ref,cost;'+body(kiosk,'purchase').replace('this thread choose_class_menu(', 'this.choose_class_menu('));
 p.menu_state=new Function('kind','var rows,tier,group,i,r,owned,perks;'+body(kiosk,'menu_state'));
 return p;
}
let p=player(0);p.menu_state('kiosk_locker');assert.equal(p.clientDvars.surHUD_credits,0);assert.equal(p.clientDvars.surHUD_cost_0,item_cost(0));
const price=item_cost(0);assert(price>0);
assert.equal(p.purchase(0),false);assert.equal(p.grants,0);assert.equal(p.survival_credit,0);
p.add_credits(price+125,'kill');assert.equal(p.score,price+125);assert.equal(p.clientDvars.surHUD_credits,price+125);assert.equal(p.survival_hud_credits.value,price+125);
assert.equal(p.purchase(0),true);assert.equal(p.grants,1);assert.equal(p.survival_credit,125);assert.equal(p.score,125);assert.equal(p.clientDvars.surHUD_credits,125);assert.equal(p.survival_hud_credits.value,125);
p.menu_state('kiosk_locker');assert.equal(p.clientDvars.surHUD_credits,125);assert.equal(p.clientDvars.surHUD_cost_0,price);
p=player(price);p.grant=false;assert.equal(p.purchase(0),false);assert.equal(p.survival_credit,price,'failed equipment grant never charges');
p=player(price);p.allow=false;assert.equal(p.purchase(0),false);assert.equal(p.survival_credit,price,'owned/full item never charges');
p=player(price);assert.equal(p.spend_credits(price+1),false);assert.equal(p.survival_credit,price);
p.menu_state('kiosk_killstreak');for(const index of menu_rows('kiosk_killstreak'))assert.equal(p.clientDvars['surHUD_cost_'+index],item_cost(index));
const session=body(kiosk,'menu_session');assert(session.includes('this.openMenu( kiosk.survival_kind )'));assert(!session.includes('openPopupMenu('));
assert(body(kiosk,'close_menu').includes('this.closeMenu()'));
assert(session.includes('thread menu_interrupt( "player_downed" )'));
assert(session.includes('thread menu_interrupt( "death" )'));
assert(body(kiosk,'menu_interrupt').includes('this.waittill( event )'));
assert(body(kiosk,'menu_credit_updates').includes('this.menu_state( kind )'));
assert(!body(survival,'player_hud').includes('this.survival_hud_credits ='),'native wallet has no duplicate upper-right credit HUD');
assert(body(survival,'player_hud').includes('this.setClientDvar( \"surHUD_credits\", this.survival_credit )'));
console.log('Actual Survival GSC: credit awards/score/dvar/HUD, authoritative prices, insufficient funds, successful purchase debit, grant/owned refusal, mouse-interactive open and matching close pass');
