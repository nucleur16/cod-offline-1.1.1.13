// Execute the actual Campaign AS bodies; only runtime/native/timer boundaries are doubled.
const fs = require('fs');
const path = require('path');
const assert = require('assert');
const asRoot = process.env.P2P_AS_ROOT || path.join(__dirname, '../zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as');
const source = fs.readFileSync(path.join(asRoot, 'codo/controllers/LobbyController.as'), 'utf8');
function body(name) {
  const start = source.indexOf('function ' + name + '(');
  assert(start >= 0, name + ' exists');
  const open = source.indexOf('{', start);
  let depth = 1, end = open + 1;
  while (depth && end < source.length) { if (source[end] === '{') depth++; if (source[end] === '}') depth--; end++; }
  return source.slice(open + 1, end - 1).replace(/var (\w+):[A-Za-z0-9_]+/g, 'var $1').replace(/super\.getSyncDP\(/g, 'this.getSyncDP(');
}
let now = 1000, dvars = {}, commands = [], readyCalls = [];
global.CFunctionContainer = {SetReadyState: value => readyCalls.push(value)};
global.BCEvent = {ROOM_STATE_CHANGED: 'room-state'};
global.ScaleformUtil = {isScaleform: true};
global.MapType = {COOP: 'story', COOP_SPEC: 'spec', COOP_SURVIVAL: 'survival'};
global.MsgBtnCount = {ONE: 1};
global.AsyncTaskID = {START_MATCH: 'start'};
global.SyncDataID = {IS_SHIP_BUILD: 'ship'};
global.CFunctionContainerCommon = {execnow: command => commands.push(command), Dvar_GetString: key => dvars[key] || '', setdvar: (key, value) => {dvars[key]=value;}};
global.getTimer = () => now;
global.TimerEvent = {TIMER: 'timer'};
global.Timer = class {
  constructor(interval) { assert.equal(interval, 250); this.running = false; }
  addEventListener(event, callback) { this.callback = callback; }
  start() { this.running = true; }
  stop() { this.running = false; }
};
function room() {
  now = 1000; commands = []; readyCalls = []; dvars = {ui_p2p_state: 'starting', ui_p2p_ready: '0', ui_p2p_enabled: '1'};
  const c = {_campaignHostTimer: null, _campaignHostMap: '', _campaignHostRequestedMap: '', _campaignHostStartPara: null,
    _campaignHostWaitStart: 0, owner: true, leaving: false, requests: [], errors: [], ready: false, broadcasts: [],
    _roomMembers: [{}], _roomObj: {isPlayList: false, mapType: MapType.COOP, currentMatchMode: {id: 'coop'},
      forces: {enableChangeMapBtn:true}, currentMap: {id: 'coop_prolog_mp_downtown'}, currentMaxPlayers: 4, currentMinPlayers: 1},
    isReady() {return this.ready;}, broadcast(id, value) {this.broadcasts.push(id);}, isRoomHost() {return this.owner;}, isRoomInfoReady() {return true;}, isLeavingRoom() {return this.leaving;},
    getSyncDP() {return false;}, request(id, args) {this.requests.push({id, args});}, showMsg(message) {this.errors.push(message);}};
  for (const name of ['isLocalCampaignRoom', 'usesLocalStartupProtocol', 'completeCampaignHostWait', 'cancelCampaignHostStart', 'beginCampaignLocalHost', 'onCampaignHostReady', 'onStartMatchHandle', 'isLocalHostRoom', 'localHostCategory', 'localHostGameType', 'onChangeReadyStateHandle', 'applyRoomReadyState','isSupportedMapMode','isManagedTrainingRoom'])
    c[name] = new Function(name === 'beginCampaignLocalHost' ? 'startPara,waitForStart,action' : name === 'applyRoomReadyState' ? 'readyToSet' : name==='isSupportedMapMode'?'mapID,modeID':'cEvt', (name === 'beginCampaignLocalHost' ? 'if(action===undefined)action="start";\n' : '') + body(name));
  // Execute the actual automatic room-entry branch, with the preceding native room hydration omitted.
  const start = source.indexOf('this.broadcast(BCEvent.ROOM_INFO_ONLY,this._roomObj);');
  const entry = source.slice(start, source.indexOf('\n      }', start)).replace('this.broadcast(BCEvent.ROOM_INFO_ONLY,this._roomObj);', '');
  c.enterCampaignRoom = new Function(entry);
  return c;
}
let c = room(); c.enterCampaignRoom();
assert.deepEqual(commands, ['p2p_room coop_prolog_mp_downtown coop coop', 'p2p_localhost 1']);
assert.equal(c._campaignHostTimer, null, 'entering room hosts early without sending StartMatch');
c.enterCampaignRoom(); assert.equal(commands.length, 2, 'room refresh is idempotent');
const parameters = {selectedDifficulty: 2};
c.onStartMatchHandle({myPara: parameters}); assert(c._campaignHostTimer.running); assert.equal(c.requests.length, 0);
assert.equal(commands.length, 4, 'Start retries/ensures the actual selected mission');
dvars.ui_p2p_ready = '1'; dvars.ui_p2p_map = 'coop_training_mp_killhouse_sh';
c.onCampaignHostReady(); assert.equal(c.requests.length, 0, 'another mission becoming ready cannot start Campaign');
dvars.ui_p2p_map = c._roomObj.currentMap.id; dvars.ui_p2p_ready = '0';
c.onCampaignHostReady(); assert.equal(c.requests.length, 0, 'matching map alone is insufficient');
dvars.ui_p2p_ready = '1'; c.onCampaignHostReady();
assert.deepEqual(c.requests, [{id: 'start', args: parameters}]); assert(!c._campaignHostTimer.running);
c.onCampaignHostReady(); assert.equal(c.requests.length, 1, 'stale callback cannot emit a second start');
for (const mutation of [x => x.owner = false, x => x._roomObj.mapType = MapType.COOP_SPEC,
  x => x._roomObj.mapType = MapType.COOP_SURVIVAL, x => x._roomObj.isPlayList = true,
  x => x._roomObj.currentMap.id = 'coop_prolog_mp_downtown;quit']) {
  c = room(); mutation(c); c.enterCampaignRoom(); assert.equal(commands.length, 0, 'only a valid owned story room auto-hosts');
}
for (const cancel of [x => x.owner = false, x => x.leaving = true, x => x._roomObj.currentMap.id = 'coop_training_mp_killhouse_sh']) {
  c = room(); c.onStartMatchHandle({myPara: parameters}); cancel(c); c.onCampaignHostReady();
  assert(!c._campaignHostTimer.running); assert.equal(c.requests.length, 0); assert.equal(c.errors.length, 0);
}
c = room(); c.onStartMatchHandle({myPara: parameters}); dvars.ui_p2p_state = 'failed'; dvars.ui_p2p_error = 'mission scripts missing';
c.onCampaignHostReady(); assert.equal(c.requests.length, 0); assert.equal(c.errors.length, 1); assert(c.errors[0].includes('mission scripts missing')); assert(!c._campaignHostTimer.running);
c = room(); c.onStartMatchHandle({myPara: parameters}); now += 180000; c.onCampaignHostReady();
assert.equal(c.requests.length, 0); assert.equal(c.errors.length, 1); assert(c.errors[0].includes('180 seconds'));
c = room(); c.onStartMatchHandle({myPara: parameters}); dvars.ui_p2p_error = 'different server is stopping'; c.onCampaignHostReady();
assert.equal(c.requests.length, 0); assert.equal(c.errors.length, 1, 'native refusal never falls through to remote StartMatch');
console.log('Campaign actual AS bodies: owner/story scope, exact mission context + enable-only commands, idempotent auto-host, ready/map gate, single start, failure, timeout and cancellation pass');

for (const [map, mode, expectedMap, category] of [
  ['mp_killhouse_sh', 'war', 'mp_killhouse_sh', 'mp'],
  ['zm_factory_sh', 'zm', 'zm_factory_sh', 'zm'],
  ['coop_survival_mp_cross_fire', 'coop', 'mp_cross_fire', 'survival'],
  ['mp_fav_tropical', 'survival', 'mp_fav_tropical', 'survival']]) {
  c = room(); c._roomObj.mapType = category; c._roomObj.currentMap.id = map; c._roomObj.currentMatchMode.id = mode;
  c.onStartMatchHandle({myPara: parameters});
  assert.deepEqual(commands, ['p2p_room ' + map + ' ' + (category === 'survival' ? 'survival' : mode) + ' ' + category]);
  assert.equal(c.requests.length, 0, 'enabled local room never requests preexisting dedicated server');
  dvars.ui_p2p_ready = '1'; dvars.ui_p2p_map = 'wrong_map'; c.onCampaignHostReady(); assert.equal(c.requests.length, 0);
  dvars.ui_p2p_map = expectedMap; c.onCampaignHostReady(); assert.equal(c.requests.length, 1);
}
c = room(); c._roomObj.mapType = 'mp'; c._roomObj.currentMap.id = 'mp_killhouse_sh'; c._roomObj.currentMatchMode.id = 'war';
c.onStartMatchHandle({myPara: parameters}); dvars.ui_p2p_enabled = '0'; c.onCampaignHostReady(); assert.equal(c.requests.length, 0); assert(!c._campaignHostTimer.running);
c = room(); c._roomObj.currentMap.id = 'invalid;quit'; c.onStartMatchHandle({myPara: parameters}); assert.equal(c.requests.length, 0); assert.equal(c.errors.length, 1);
c = room(); c.onStartMatchHandle({myPara: parameters}); c._roomObj.currentMatchMode.id = 'war'; c.onCampaignHostReady(); assert.equal(c.requests.length, 0); assert(!c._campaignHostTimer.running);
c = room(); c._roomObj.mapType = 'mp'; c._roomObj.currentMap.id = 'mp_killhouse_sh'; c._roomObj.currentMatchMode.id = 'war'; dvars.ui_p2p_enabled = '0';
c.onStartMatchHandle({myPara: parameters}); assert.equal(commands.length, 0); assert.equal(c.requests.length, 1, 'disabled hosting retains the ordinary lobby route');
console.log('All normal enabled local rooms gate StartMatch on exact ready host; Survival normalizes its legacy session ID; disable/mode changes cancel; invalid local context cannot fall back');

// Native Survival can retain the story-shaped room metadata. It must never inherit Campaign auto-enable.
c = room(); c._roomObj.mapType = MapType.COOP; c._roomObj.currentMap.id = 'coop_survival_mp_fav_tropical'; c._roomObj.currentMatchMode.id = 'coop'; dvars.ui_p2p_enabled = '0';
c.enterCampaignRoom(); assert.equal(commands.length, 0, 'legacy Survival room does not auto-enable localhost');
c.onStartMatchHandle({myPara: parameters}); assert.equal(commands.length, 0); assert.equal(c.requests.length, 1, 'disabled legacy Survival keeps ordinary lobby flow');
c = room(); c._roomObj.mapType = MapType.COOP; c._roomObj.currentMap.id = 'coop_survival_mp_fav_tropical'; c._roomObj.currentMatchMode.id = 'coop';
c.onStartMatchHandle({myPara: parameters}); assert.deepEqual(commands, ['p2p_room coop_survival_mp_fav_tropical survival survival']); assert.equal(c.requests.length, 0);
dvars.ui_p2p_ready = '1'; dvars.ui_p2p_map = 'mp_fav_tropical'; c.onCampaignHostReady(); assert.equal(c.requests.length, 1);
console.log('Story-shaped legacy Survival metadata respects its own enabled toggle and never receives Campaign default enable');

c = room(); c.onStartMatchHandle({myPara: parameters});
dvars.ui_p2p_state = 'ready'; dvars.ui_p2p_error = 'Waiting to publish the local host route.'; dvars.ui_p2p_ready = '0'; dvars.ui_p2p_map = c._roomObj.currentMap.id;
c.onCampaignHostReady(); assert.equal(c.requests.length, 0); assert.equal(c.errors.length, 0); assert(c._campaignHostTimer.running, 'transient local-route publication waits without cancelling start');
dvars.ui_p2p_error = ''; dvars.ui_p2p_ready = '1'; c.onCampaignHostReady();
assert.equal(c.requests.length, 1); assert.equal(c.errors.length, 0); assert(!c._campaignHostTimer.running);
console.log('Transient local-route publication retries retain pending start and recover when matching host becomes ready');

// The visible normal-room Ready control must defer its actual native Ready bit, not invent START_MATCH.
c = room(); c.onChangeReadyStateHandle({});
assert.equal(readyCalls.length, 0); assert.equal(c.requests.length, 0); assert(c._campaignHostTimer.running); assert.equal(c._roomObj.forces.enableChangeMapBtn, true);
dvars.ui_p2p_ready = '1'; dvars.ui_p2p_map = 'wrong_map'; c.onCampaignHostReady(); assert.equal(readyCalls.length, 0);
dvars.ui_p2p_map = c._roomObj.currentMap.id; c.onCampaignHostReady();
assert.deepEqual(readyCalls, [true]); assert.equal(c.requests.length, 0); assert.deepEqual(c.broadcasts, ['room-state']); assert.equal(c._roomObj.forces.enableChangeMapBtn, false);
c.onCampaignHostReady(); assert.equal(readyCalls.length, 1);
for (const cancel of [x => x.owner = false, x => x.leaving = true, x => x._roomObj.currentMap.id = 'coop_training_mp_killhouse_sh', x => dvars.ui_p2p_enabled = '0']) {
  c = room(); c.onChangeReadyStateHandle({}); cancel(c); c.onCampaignHostReady(); assert.equal(readyCalls.length, 0); assert.equal(c.requests.length, 0); assert(!c._campaignHostTimer.running);
}
c = room(); c.onChangeReadyStateHandle({}); dvars.ui_p2p_error = 'No host route'; c.onCampaignHostReady(); assert.equal(readyCalls.length, 0); assert.equal(c.requests.length, 0); assert.equal(c.errors.length, 1);
c = room(); c.ready = true; c.onChangeReadyStateHandle({}); assert.deepEqual(readyCalls, [false]); assert.equal(commands.length, 0); assert.equal(c._roomObj.forces.enableChangeMapBtn, true);
c = room(); c._roomObj.mapType = 'mp'; c._roomObj.currentMap.id = 'mp_killhouse_sh'; c._roomObj.currentMatchMode.id = 'war'; dvars.ui_p2p_enabled = '0'; c.onChangeReadyStateHandle({}); assert.deepEqual(readyCalls, [true]); assert.equal(commands.length, 0);
console.log('Visible Ready handler defers native Ready exactly once to matching host, preserves map-controls/broadcast, immediate unready, disabled normal flow, and cancels failures/disable/leave/map changes');

for (const [map,mode,readyMap] of [['mp_killhouse_sh','war','mp_killhouse_sh'],['zm_factory_sh','zm','zm_factory_sh'],['coop_survival_mp_fav_tropical','coop','mp_fav_tropical']]) {
  c = room(); c._roomObj.currentMap.id = map; c._roomObj.currentMatchMode.id = mode; c.onChangeReadyStateHandle({}); assert.equal(readyCalls.length, 0);
  dvars.ui_p2p_ready = '1'; dvars.ui_p2p_map = readyMap; c.onCampaignHostReady(); assert.deepEqual(readyCalls, [true]); assert.equal(c.requests.length, 0);
}
c = room(); c.onChangeReadyStateHandle({}); now += 180000; c.onCampaignHostReady(); assert.equal(readyCalls.length, 0); assert.equal(c.requests.length, 0); assert.equal(c.errors.length, 1);

// The new protocol uses only a genuine Ready/Start action as startup intent.
// These execute the same AS bodies above; native authority is not mocked Ready.
c = room(); dvars.ui_p2p_startup_protocol = '1'; c._localHostPendingAction = '';
c.enterCampaignRoom();
assert.deepEqual(commands, ['p2p_room coop_prolog_mp_downtown coop coop', 'p2p_localhost 1']);
assert.equal(c._campaignHostTimer, null, 'Create arms without a startup wait or DW Ready');
commands = []; c.onChangeReadyStateHandle({});
assert.deepEqual(commands, ['p2p_start_intent ready coop_prolog_mp_downtown coop coop']);
assert.deepEqual(readyCalls, []); assert.equal(c.requests.length, 0);
const firstWait = c._campaignHostWaitStart;
now += 500; c.onStartMatchHandle({myPara: parameters});
assert.equal(commands.length, 1, 'duplicate pending Start cannot rewrite context or renew the attempt');
assert.equal(c._campaignHostWaitStart, firstWait);
c.onChangeReadyStateHandle({});
assert.deepEqual(commands, ['p2p_start_intent ready coop_prolog_mp_downtown coop coop', 'p2p_cancel_start']);
assert(!c._campaignHostTimer.running); assert.deepEqual(readyCalls, [], 'second pending Ready cancels without inventing unready');
c.onCampaignHostReady(); assert.equal(commands.length, 2, 'stale timer after cancellation is inert');
c.onChangeReadyStateHandle({});
assert.equal(commands[2], 'p2p_start_intent ready coop_prolog_mp_downtown coop coop');
dvars.ui_p2p_ready = '1'; dvars.ui_p2p_map = c._roomObj.currentMap.id;
c.onCampaignHostReady(); assert.deepEqual(readyCalls, [true]);
assert.equal(commands.length, 3, 'success clears only UI wait and does not revoke live native startup');
c.onCampaignHostReady(); assert.equal(commands.length, 3, 'stale success callback cannot revoke');
c.onStartMatchHandle({myPara: parameters});
assert.equal(commands[3], 'p2p_start_intent start coop_prolog_mp_downtown coop coop');
c.onCampaignHostReady(); assert.equal(c.requests.length, 1); assert.equal(commands.length, 4);
c.ready = true; c.onChangeReadyStateHandle({});
assert.equal(commands[4], 'p2p_cancel_start'); assert.deepEqual(readyCalls, [true, false]);
for (const mutation of [x => x.owner = false, x => x.leaving = true, x => x._roomObj.currentMap.id = 'mp_rust', x => x._roomObj.currentMatchMode.id = 'war']) {
  c = room(); dvars.ui_p2p_startup_protocol='1'; c._localHostPendingAction='';
  c.onChangeReadyStateHandle({}); mutation(c); c.onCampaignHostReady();
  assert.equal(commands.at(-1), 'p2p_cancel_start'); assert.deepEqual(readyCalls, []); assert.equal(c.requests.length, 0);
}
c = room(); dvars.ui_p2p_startup_protocol='-1'; c._localHostPendingAction='';
c.onChangeReadyStateHandle({}); dvars.ui_p2p_state='failed'; dvars.ui_p2p_error='invalid protocol'; c.onCampaignHostReady();
assert.deepEqual(readyCalls, []); assert.equal(c.requests.length, 0); assert.equal(commands.at(-1), 'p2p_cancel_start');
for (const [map, mode, category] of [['mp_killhouse_sh','war','mp'],['zm_factory_sh','zm','zm'],['coop_survival_mp_cross_fire','coop','survival']]) {
  c = room(); dvars.ui_p2p_startup_protocol='1'; c._localHostPendingAction='';
  c._roomObj.currentMap.id=map; c._roomObj.currentMatchMode.id=mode;
  c.onChangeReadyStateHandle({});
  assert.deepEqual(commands, ['p2p_start_intent ready '+map+' '+(category==='survival'?'survival':mode)+' '+category]);
  assert.deepEqual(readyCalls, []);
}
console.log('New startup protocol: Create arms; genuine Ready/Start produces intent; duplicate waits preserve deadline; pending Ready cancels; success never revokes; stale callbacks/context/owner/leave and malformed mode fail closed');

// Execute the actual leave handler through its room-reset boundary. The rest
// belongs to unrelated lobby/avatar/navigation rendering, not native lifetime.
c = room(); dvars.ui_p2p_startup_protocol='1'; c._localHostPendingAction='';
c.onChangeReadyStateHandle({}); commands=[];
new Function('cRes',body('bind_onRoomLeft').split('this.resetRoom();')[0]).call(c,{});
assert.deepEqual(commands,['p2p_cancel_start','p2p_localhost 0']);
assert(!c._campaignHostTimer.running); assert.equal(c._campaignHostRequestedMap,'');
console.log('Actual room-leave prefix cancels pending native attempt and explicitly deselects its local route before reset');
