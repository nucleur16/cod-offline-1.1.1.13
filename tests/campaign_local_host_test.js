// Execute the actual LobbyController AS bodies through the room's own entry points (room entry, the visible Ready
// control, Start, room leave); only runtime/native/timer boundaries are doubled.
//
// docs/P2P_DEFERRED_HOST_START.md: in startup protocols 0-2 Ready and Start proceed at once (the embedded server is
// started by Kernel33 when the match launches) and the timer is only a failure watch; protocol 3 (production) keeps
// its waiting gate. tests/local_play_deferred_test.js drives beginDeferredLocalHost directly; this file keeps the
// Campaign default, the room scope rules, the per-category command context and the production gate.
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
let now = 1000, dvars = {}, commands = [], readyCalls = [], events = [], current = null;
global.CFunctionContainer = {SetReadyState: value => { readyCalls.push(value); events.push('ready:' + value); if (current) current.ready = value; }};
global.BCEvent = {ROOM_STATE_CHANGED: 'room-state'};
global.ScaleformUtil = {isScaleform: true};
global.MapType = {COOP: 'story', COOP_SPEC: 'spec', COOP_SURVIVAL: 'survival'};
global.MsgBtnCount = {ONE: 1};
global.AsyncTaskID = {START_MATCH: 'start'};
global.SyncDataID = {IS_SHIP_BUILD: 'ship'};
global.int = x => Number(x)|0;
global.CFunctionContainerCommon = {execnow: command => { commands.push(command); events.push('cmd:' + command); }, Dvar_GetString: key => dvars[key] || '', setdvar: (key, value) => {dvars[key]=value;}};
global.getTimer = () => now;
global.TimerEvent = {TIMER: 'timer'};
global.Timer = class {
  constructor(interval) { assert.equal(interval, 250); this.running = false; }
  addEventListener(event, callback) { this.callback = callback; }
  start() { this.running = true; }
  stop() { this.running = false; }
};
const PARAMETERS = {beginCampaignLocalHost: 'startPara,waitForStart,action', beginDeferredLocalHost: 'startPara,action',
  applyRoomReadyState: 'readyToSet', isSupportedMapMode: 'mapID,modeID'};
const BODIES = ['isLocalCampaignRoom', 'usesLocalStartupProtocol', 'isProductionLocalRoom', 'localIntentRoomKey', 'localHostContext',
  'completeCampaignHostWait', 'cancelCampaignHostStart', 'beginCampaignLocalHost', 'beginDeferredLocalHost', 'onCampaignHostReady',
  'onStartMatchHandle', 'isLocalHostRoom', 'localHostCategory', 'localHostGameType', 'onChangeReadyStateHandle', 'applyRoomReadyState',
  'isSupportedMapMode', 'isManagedTrainingRoom'];
function room(protocol) {
  now = 1000; commands = []; readyCalls = []; events = [];
  dvars = {ui_p2p_state: 'armed', ui_p2p_ready: '0', ui_p2p_enabled: '1', ui_p2p_startup_protocol: protocol === undefined ? '0' : protocol};
  const c = {_campaignHostTimer: null, _campaignHostMap: '', _campaignHostSelectedMap: '', _campaignHostContext: '', _campaignHostRequestedMap: '',
    _campaignHostStartPara: null, _campaignHostWaitStart: 0, _localHostPendingAction: '', _campaignHostRoomKey: '', _campaignHostGeneration: '',
    owner: true, leaving: false, requests: [], errors: [], ready: false, broadcasts: [],
    _roomMembers: [{}], _roomObj: {isPlayList: false, mapType: MapType.COOP, index: 7, name: 'edtest', currentMatchMode: {id: 'coop'},
      forces: {enableChangeMapBtn:true}, currentMap: {id: 'coop_prolog_mp_downtown'}, currentMaxPlayers: 4, currentMinPlayers: 1,currentLevel:1},
    isReady() {return this.ready;}, broadcast(id, value) {this.broadcasts.push(id);}, isRoomHost() {return this.owner;}, isRoomInfoReady() {return true;}, isLeavingRoom() {return this.leaving;},
    getSyncDP() {return false;}, request(id, args) {this.requests.push({id, args}); events.push('request:' + id);}, showMsg(message) {this.errors.push(message);}};
  for (const name of BODIES)
    c[name] = new Function(PARAMETERS[name] || 'cEvt', (name === 'beginCampaignLocalHost' ? 'if(action===undefined)action="start";\n' : '') + body(name));
  // Execute the actual automatic room-entry branch, with the preceding native room hydration omitted.
  const start = source.indexOf('this.broadcast(BCEvent.ROOM_INFO_ONLY,this._roomObj);');
  const entry = source.slice(start, source.indexOf('\n      }', start)).replace('this.broadcast(BCEvent.ROOM_INFO_ONLY,this._roomObj);', '');
  c.enterCampaignRoom = new Function(entry);
  current = c;
  return c;
}
const watching = c => c._campaignHostTimer !== null && c._campaignHostTimer.running;
const parameters = {selectedDifficulty: 2};

// ---- Protocol 0: the Campaign default and Start ----
let c = room(); c.enterCampaignRoom();
assert.deepEqual(commands, ['p2p_room coop_prolog_mp_downtown coop coop', 'p2p_localhost 1']);
assert.equal(c._campaignHostTimer, null, 'entering a story room arms the local choice without a watch or StartMatch');
c.enterCampaignRoom(); assert.equal(commands.length, 2, 'room refresh is idempotent');
c.onStartMatchHandle({myPara: parameters});
assert.deepEqual(c.requests, [{id: 'start', args: parameters}], 'Start is issued at once: no running host is awaited');
assert.deepEqual(events.slice(2), ['cmd:p2p_room coop_prolog_mp_downtown coop coop', 'cmd:p2p_localhost 1', 'request:start'],
  'Start republishes the actual selected mission before the request');
assert(watching(c)); assert.equal(c._localHostPendingAction, 'launch'); assert.deepEqual(readyCalls, []);
c.onCampaignHostReady(); assert(watching(c)); assert.equal(c.errors.length, 0, 'an armed host that is not running is not an error');
dvars.ui_p2p_launching = '1'; dvars.ui_p2p_state = 'starting'; c.onCampaignHostReady(); assert(watching(c));
dvars.ui_p2p_ready = '1'; dvars.ui_p2p_state = 'ready'; dvars.ui_p2p_map = c._roomObj.currentMap.id; c.onCampaignHostReady();
assert(!watching(c)); assert.equal(c.requests.length, 1, 'the host becoming ready ends the watch and never emits a second start');
c.onCampaignHostReady(); assert.equal(c.requests.length, 1, 'a stale callback is inert'); assert.equal(c.errors.length, 0);
for (const mutation of [x => x.owner = false, x => x._roomObj.mapType = MapType.COOP_SPEC,
  x => x._roomObj.mapType = MapType.COOP_SURVIVAL, x => x._roomObj.isPlayList = true,
  x => x._roomObj.currentMap.id = 'coop_prolog_mp_downtown;quit']) {
  c = room(); mutation(c); c.enterCampaignRoom(); assert.equal(commands.length, 0, 'only a valid owned story room auto-hosts');
}
// A pending launch is revoked silently by a lost seat, leaving or another mission; the start already issued is not repeated.
for (const revoke of [x => x.owner = false, x => x.leaving = true, x => x._roomObj.currentMap.id = 'coop_training_mp_killhouse_sh']) {
  c = room(); c.onStartMatchHandle({myPara: parameters}); revoke(c); c.onCampaignHostReady();
  assert(!watching(c)); assert.equal(c._localHostPendingAction, ''); assert.equal(c.requests.length, 1); assert.equal(c.errors.length, 0);
}
c = room(); c.onStartMatchHandle({myPara: parameters}); dvars.ui_p2p_state = 'failed'; dvars.ui_p2p_error = 'mission scripts missing';
c.onCampaignHostReady(); assert.equal(c.errors.length, 1); assert(c.errors[0].includes('mission scripts missing')); assert(!watching(c));
assert.equal(c.requests.length, 1); assert.deepEqual(readyCalls, [], 'a failed Start has no Ready to take back');
c.onCampaignHostReady(); assert.equal(c.errors.length, 1, 'the failure is reported once');
c = room(); c.onStartMatchHandle({myPara: parameters}); dvars.ui_p2p_state = 'failed';
c.onCampaignHostReady(); assert(c.errors[0].includes('the local host did not start'));
// No 180 s readiness deadline any more: nothing is being waited for. The idle watch is simply dropped after 10 minutes.
c = room(); c.onStartMatchHandle({myPara: parameters}); now += 180000; c.onCampaignHostReady();
assert(watching(c)); assert.equal(c.errors.length, 0);
now += 420000; c.onCampaignHostReady(); assert(!watching(c)); assert.equal(c.errors.length, 0); assert.equal(c.requests.length, 1);
c = room(); c.onStartMatchHandle({myPara: parameters}); dvars.ui_p2p_error = 'different server is stopping'; c.onCampaignHostReady();
assert.equal(c.errors.length, 1, 'a native refusal is reported'); assert.equal(c.requests.length, 1); assert(!watching(c));
console.log('Campaign actual AS bodies (protocol 0): owner/story scope, idempotent auto-enable, Start issued at once after the exact mission context, failure watch, single start, no readiness deadline, silent revocation');

for (const [map, mode, category] of [
  ['mp_killhouse_sh', 'war', 'mp'],
  ['zm_factory_sh', 'zm', 'zm'],
  ['coop_survival_mp_cross_fire', 'coop', 'survival'],
  ['mp_fav_tropical', 'survival', 'survival']]) {
  c = room(); c._roomObj.mapType = category; c._roomObj.currentMap.id = map; c._roomObj.currentMatchMode.id = mode;
  c.onStartMatchHandle({myPara: parameters});
  assert.deepEqual(events, ['cmd:p2p_room ' + map + ' ' + (category === 'survival' ? 'survival' : mode) + ' ' + category, 'request:start'],
    'an enabled local room publishes its own context, never auto-enables, and starts at once');
  // The watch compares nothing with the host's map: whatever answers, no second start can be produced here.
  dvars.ui_p2p_ready = '1'; dvars.ui_p2p_map = 'wrong_map'; c.onCampaignHostReady(); assert.equal(c.requests.length, 1); assert(!watching(c));
}
c = room(); c._roomObj.mapType = 'mp'; c._roomObj.currentMap.id = 'mp_killhouse_sh'; c._roomObj.currentMatchMode.id = 'war';
c.onStartMatchHandle({myPara: parameters}); dvars.ui_p2p_enabled = '0'; c.onCampaignHostReady(); assert(!watching(c)); assert.equal(c.errors.length, 0);
c = room(); c._roomObj.currentMap.id = 'invalid;quit'; c.onStartMatchHandle({myPara: parameters});
assert.equal(c.requests.length, 0, 'an invalid local context can neither start nor fall back'); assert.equal(c.errors.length, 1); assert.equal(commands.length, 0);
c = room(); c.onStartMatchHandle({myPara: parameters}); c._roomObj.currentMatchMode.id = 'war'; c.onCampaignHostReady(); assert(!watching(c)); assert.equal(c.errors.length, 0);
c = room(); c._roomObj.mapType = 'mp'; c._roomObj.currentMap.id = 'mp_killhouse_sh'; c._roomObj.currentMatchMode.id = 'war'; dvars.ui_p2p_enabled = '0';
c.onStartMatchHandle({myPara: parameters}); assert.equal(commands.length, 0); assert.equal(c.requests.length, 1, 'disabled hosting retains the ordinary lobby route');
assert.equal(c._campaignHostTimer, null);
console.log('All normal enabled local rooms publish the exact context and start at once; Survival normalizes its legacy session ID; disable/mode changes end the watch; invalid local context cannot fall back');

// Native Survival can retain the story-shaped room metadata. It must never inherit Campaign auto-enable.
c = room(); c._roomObj.mapType = MapType.COOP; c._roomObj.currentMap.id = 'coop_survival_mp_fav_tropical'; c._roomObj.currentMatchMode.id = 'coop'; dvars.ui_p2p_enabled = '0';
c.enterCampaignRoom(); assert.equal(commands.length, 0, 'legacy Survival room does not auto-enable localhost');
c.onStartMatchHandle({myPara: parameters}); assert.equal(commands.length, 0); assert.equal(c.requests.length, 1, 'disabled legacy Survival keeps ordinary lobby flow');
c = room(); c._roomObj.mapType = MapType.COOP; c._roomObj.currentMap.id = 'coop_survival_mp_fav_tropical'; c._roomObj.currentMatchMode.id = 'coop';
c.onStartMatchHandle({myPara: parameters});
assert.deepEqual(events, ['cmd:p2p_room coop_survival_mp_fav_tropical survival survival', 'request:start']);
console.log('Story-shaped legacy Survival metadata respects its own enabled toggle and never receives Campaign default enable');

c = room(); c.onStartMatchHandle({myPara: parameters});
dvars.ui_p2p_state = 'ready'; dvars.ui_p2p_error = 'Waiting to publish the local host route.'; dvars.ui_p2p_ready = '0'; dvars.ui_p2p_map = c._roomObj.currentMap.id;
c.onCampaignHostReady(); assert.equal(c.errors.length, 0); assert(watching(c), 'transient local-route publication is not a failure');
dvars.ui_p2p_error = ''; dvars.ui_p2p_ready = '1'; c.onCampaignHostReady();
assert.equal(c.requests.length, 1); assert.equal(c.errors.length, 0); assert(!watching(c));
console.log('Transient local-route publication retries are not reported and the watch ends when the host is ready');

// The visible normal-room Ready control sets the actual native Ready bit at once; it never invents START_MATCH.
c = room(); c.onChangeReadyStateHandle({});
assert.deepEqual(readyCalls, [true]); assert.equal(c.requests.length, 0); assert(watching(c));
assert.deepEqual(events, ['cmd:p2p_room coop_prolog_mp_downtown coop coop', 'cmd:p2p_localhost 1', 'ready:true']);
assert.deepEqual(c.broadcasts, ['room-state']); assert.equal(c._roomObj.forces.enableChangeMapBtn, false);
dvars.ui_p2p_ready = '1'; dvars.ui_p2p_map = c._roomObj.currentMap.id; c.onCampaignHostReady();
assert.deepEqual(readyCalls, [true], 'host readiness never re-applies Ready'); assert(!watching(c));
c.onCampaignHostReady(); assert.deepEqual(readyCalls, [true]);
for (const revoke of [x => x.owner = false, x => x.leaving = true, x => x._roomObj.currentMap.id = 'coop_training_mp_killhouse_sh', x => dvars.ui_p2p_enabled = '0']) {
  c = room(); c.onChangeReadyStateHandle({}); revoke(c); c.onCampaignHostReady();
  assert.deepEqual(readyCalls, [true]); assert.equal(c.requests.length, 0); assert.equal(c.errors.length, 0); assert(!watching(c));
}
c = room(); c.onChangeReadyStateHandle({}); dvars.ui_p2p_error = 'No host route'; c.onCampaignHostReady();
assert.deepEqual(readyCalls, [true, false], 'a failed launch takes Ready back'); assert.equal(c.requests.length, 0); assert.equal(c.errors.length, 1);
assert.equal(c._roomObj.forces.enableChangeMapBtn, true);
c = room(); c.onChangeReadyStateHandle({}); c.onChangeReadyStateHandle({});
assert.deepEqual(readyCalls, [true, false], 'a second click is the ordinary unready'); assert(!watching(c)); assert.equal(commands.length, 2);
assert.equal(c._roomObj.forces.enableChangeMapBtn, true);
c = room(); c.ready = true; c.onChangeReadyStateHandle({}); assert.deepEqual(readyCalls, [false]); assert.equal(commands.length, 0); assert.equal(c._roomObj.forces.enableChangeMapBtn, true);
c = room(); c._roomObj.mapType = 'mp'; c._roomObj.currentMap.id = 'mp_killhouse_sh'; c._roomObj.currentMatchMode.id = 'war'; dvars.ui_p2p_enabled = '0'; c.onChangeReadyStateHandle({}); assert.deepEqual(readyCalls, [true]); assert.equal(commands.length, 0);
assert.equal(c._campaignHostTimer, null);
c = room(); c._roomObj.currentMap.id = 'invalid;quit'; c.onChangeReadyStateHandle({}); assert.deepEqual(readyCalls, []); assert.equal(c.errors.length, 1);
for (const [map,mode,context] of [['mp_killhouse_sh','war','war mp'],['zm_factory_sh','zm','zm zm'],['coop_survival_mp_fav_tropical','coop','survival survival']]) {
  c = room(); c._roomObj.currentMap.id = map; c._roomObj.currentMatchMode.id = mode; c.onChangeReadyStateHandle({});
  assert.deepEqual(events, ['cmd:p2p_room ' + map + ' ' + context, 'ready:true']); assert.equal(c.requests.length, 0);
}
c = room(); c.onChangeReadyStateHandle({}); now += 180000; c.onCampaignHostReady();
assert.deepEqual(readyCalls, [true]); assert.equal(c.errors.length, 0, 'Ready has no host deadline'); assert(watching(c));
console.log('Visible Ready handler (protocol 0) applies native Ready once and at once, preserves map-controls/broadcast, ordinary unready and disabled flow; a failed launch takes Ready back; revocations are silent');

// Protocols 1 and 2: a genuine Ready/Start sends the startup intent (natively it only permits the launch) and proceeds at once.
c = room('1');
c.enterCampaignRoom();
assert.deepEqual(commands, ['p2p_room coop_prolog_mp_downtown coop coop', 'p2p_localhost 1']);
assert.equal(c._campaignHostTimer, null, 'Create arms without a watch or DW Ready');
commands = []; events = []; c.onChangeReadyStateHandle({});
assert.deepEqual(events, ['cmd:p2p_start_intent ready coop_prolog_mp_downtown coop coop', 'ready:true']);
assert.equal(c.requests.length, 0);
now += 500; c.onStartMatchHandle({myPara: parameters});
assert.deepEqual(events.slice(2), ['cmd:p2p_start_intent start coop_prolog_mp_downtown coop coop', 'request:start'], 'Ready then Start: the Start intent precedes its request');
c.onChangeReadyStateHandle({});
assert.equal(commands.at(-1), 'p2p_cancel_start', 'unready revokes the native permission'); assert.deepEqual(readyCalls, [true, false]);
assert(!watching(c)); const settled = commands.length;
c.onCampaignHostReady(); assert.equal(commands.length, settled, 'stale timer after cancellation is inert');
c.onChangeReadyStateHandle({});
assert.equal(commands.at(-1), 'p2p_start_intent ready coop_prolog_mp_downtown coop coop'); assert.deepEqual(readyCalls, [true, false, true]);
dvars.ui_p2p_ready = '1'; dvars.ui_p2p_map = c._roomObj.currentMap.id;
c.onCampaignHostReady(); assert.equal(commands.length, settled + 1, 'a ready host ends only the UI watch and does not revoke the live native startup');
c.onCampaignHostReady(); assert.equal(commands.length, settled + 1, 'stale success callback cannot revoke');
for (const mutation of [x => x.owner = false, x => x.leaving = true, x => x._roomObj.currentMap.id = 'mp_rust', x => x._roomObj.currentMatchMode.id = 'war']) {
  c = room('1');
  c.onChangeReadyStateHandle({}); mutation(c); c.onCampaignHostReady();
  assert.equal(commands.at(-1), 'p2p_cancel_start'); assert.deepEqual(readyCalls, [true]); assert.equal(c.requests.length, 0); assert(!watching(c));
}
c = room('-1');
c.onChangeReadyStateHandle({}); dvars.ui_p2p_state='failed'; dvars.ui_p2p_error='invalid protocol'; c.onCampaignHostReady();
assert.deepEqual(readyCalls, [true, false]); assert.equal(c.requests.length, 0); assert.equal(commands.at(-1), 'p2p_cancel_start');
assert.equal(c.errors.length, 1); assert(c.errors[0].includes('invalid protocol'));
for (const [map, mode, category] of [['mp_killhouse_sh','war','mp'],['zm_factory_sh','zm','zm'],['coop_survival_mp_cross_fire','coop','survival']]) {
  c = room('1');
  c._roomObj.currentMap.id=map; c._roomObj.currentMatchMode.id=mode;
  c.onChangeReadyStateHandle({});
  assert.deepEqual(events, ['cmd:p2p_start_intent ready '+map+' '+(category==='survival'?'survival':mode)+' '+category, 'ready:true']);
}
console.log('Startup protocol 1: Create arms; genuine Ready/Start send their intent first and proceed at once; unready and changed context/owner/leave revoke; success never revokes; a native failure takes Ready back');

// Execute the actual leave handler through its room-reset boundary. The rest
// belongs to unrelated lobby/avatar/navigation rendering, not native lifetime.
c = room('1');
c.onChangeReadyStateHandle({}); commands=[];
new Function('cRes',body('bind_onRoomLeft').split('this.resetRoom();')[0]).call(c,{});
assert.deepEqual(commands,['p2p_cancel_start','p2p_localhost 0']);
assert(!watching(c)); assert.equal(c._campaignHostRequestedMap,''); assert.equal(c._localHostPendingAction,'');
c = room('0'); c.onChangeReadyStateHandle({}); commands=[];
new Function('cRes',body('bind_onRoomLeft').split('this.resetRoom();')[0]).call(c,{});
assert.deepEqual(commands,[],'the legacy protocol leaves disarming to the room helper'); assert(!watching(c));
console.log('Actual room-leave prefix cancels the pending native permission and explicitly deselects its local route before reset');

c=room('2');
c._roomObj.currentMap.id='zm_factory_sh';c._roomObj.currentMatchMode.id='zm';c._roomObj.currentLevel=0;
c.onChangeReadyStateHandle({});
assert.deepEqual(events,['cmd:p2p_start_intent ready zm_factory_sh zm zm 1 0','ready:true']);
c._roomObj.currentLevel=3;c.onCampaignHostReady();
assert.equal(commands.at(-1),'p2p_cancel_start','a changed tier revokes the permission that was given for the old one');assert(!watching(c));
c.onChangeReadyStateHandle({});assert.deepEqual(readyCalls,[true,false]);
c.onChangeReadyStateHandle({});assert.equal(commands.at(-1),'p2p_start_intent ready zm_factory_sh zm zm 1 3');assert.deepEqual(readyCalls,[true,false,true]);
for(const bad of [-1,4,1.5,NaN,Infinity,'3',true,undefined]) {
 c=room('2');
 c._roomObj.currentMap.id='zm_factory_sh';c._roomObj.currentMatchMode.id='zm';c._roomObj.currentLevel=bad;
 c.onChangeReadyStateHandle({});assert.deepEqual(commands,['p2p_cancel_start']);assert.deepEqual(readyCalls,[],'a malformed tier never becomes Ready');
 c.onStartMatchHandle({myPara:parameters});assert.equal(c.requests.length,0,'nor a Start');
}
c=room('2');
c._roomObj.currentMap.id='mp_rust';c._roomObj.currentMatchMode.id='war';c._roomObj.currentLevel=17;
c.onChangeReadyStateHandle({});assert.deepEqual(commands,['p2p_start_intent ready mp_rust war mp 0 -1']);
console.log('V2 intent preserves genuine 0/Endless 3, revokes a changed tier, refuses a malformed tier and keeps the MP league absent');

// ---- Protocol 3 (production direct-v1): unchanged. Ready/Start are withheld until the SDK-sequenced initial handoff
// of the generation that this attempt observed, for the selected mission. ----
function production() {
  const p = room('3'); dvars.ui_p2p_state = 'starting'; return p;
}
c = production(); c.onChangeReadyStateHandle({});
assert.deepEqual(commands, ['p2p_start_intent ready coop_prolog_mp_downtown coop coop 1 1'], 'production sends only the intent');
assert.deepEqual(readyCalls, [], 'production Ready is withheld'); assert(watching(c)); assert.equal(c._localHostPendingAction, 'ready');
const firstWait = c._campaignHostWaitStart;
now += 500; c.onStartMatchHandle({myPara: parameters});
assert.equal(commands.length, 1, 'duplicate pending Start cannot rewrite context or renew the attempt');
assert.equal(c._campaignHostWaitStart, firstWait); assert.equal(c.requests.length, 0);
dvars.ui_p2p_ready = '1'; dvars.ui_p2p_map = c._roomObj.currentMap.id; c.onCampaignHostReady();
assert.deepEqual(readyCalls, [], 'legacy readiness alone does not open the production gate');
dvars.ui_p2p_generation = '5'; dvars.ui_p2p_initial_handoff = '0'; c.onCampaignHostReady();
assert.deepEqual(readyCalls, []); assert.equal(c._campaignHostGeneration, '5');
dvars.ui_p2p_generation = '6'; dvars.ui_p2p_initial_handoff = '1'; c.onCampaignHostReady();
assert.deepEqual(readyCalls, [], 'another generation\'s handoff cannot complete this attempt');
dvars.ui_p2p_generation = '5'; dvars.ui_p2p_map = 'coop_training_mp_killhouse_sh'; c.onCampaignHostReady();
assert.deepEqual(readyCalls, [], 'another mission cannot complete this attempt');
dvars.ui_p2p_map = c._roomObj.currentMap.id; c.onCampaignHostReady();
assert.deepEqual(readyCalls, [true]); assert(!watching(c)); assert.equal(c.requests.length, 0);
assert.equal(commands.length, 1, 'success clears only the UI wait and does not revoke live native startup');
c.onCampaignHostReady(); assert.deepEqual(readyCalls, [true], 'stale success callback is inert');
c.onStartMatchHandle({myPara: parameters});
assert.equal(commands[1], 'p2p_start_intent start coop_prolog_mp_downtown coop coop 1 1'); assert.equal(c.requests.length, 0, 'production Start is withheld');
dvars.ui_p2p_generation = '7'; c.onCampaignHostReady(); assert.deepEqual(c.requests, [{id: 'start', args: parameters}]);
c.onCampaignHostReady(); assert.equal(c.requests.length, 1);
c.onChangeReadyStateHandle({});
assert.equal(commands.at(-1), 'p2p_cancel_start'); assert.deepEqual(readyCalls, [true, false]);
c = production(); c.onChangeReadyStateHandle({}); c.onChangeReadyStateHandle({});
assert.deepEqual(commands, ['p2p_start_intent ready coop_prolog_mp_downtown coop coop 1 1', 'p2p_cancel_start']);
assert(!watching(c)); assert.deepEqual(readyCalls, [], 'second pending Ready cancels without inventing unready');
c.onCampaignHostReady(); assert.equal(commands.length, 2, 'stale timer after cancellation is inert');
for (const cancel of [x => x.leaving = true, x => x._roomObj.currentMap.id = 'mp_rust', x => x._roomObj.currentLevel = 2, x => dvars.ui_p2p_enabled = '0']) {
  c = production(); c.onStartMatchHandle({myPara: parameters}); cancel(c); c.onCampaignHostReady();
  assert(!watching(c)); assert.equal(c.requests.length, 0); assert.equal(c.errors.length, 0); assert.deepEqual(readyCalls, []);
}
c = production(); c.onStartMatchHandle({myPara: parameters}); dvars.ui_p2p_state = 'failed'; dvars.ui_p2p_error = 'mission scripts missing';
c.onCampaignHostReady(); assert.equal(c.requests.length, 0); assert.equal(c.errors.length, 1); assert(c.errors[0].includes('mission scripts missing'));
assert.equal(commands.at(-1), 'p2p_cancel_start');
c = production(); c.onStartMatchHandle({myPara: parameters}); now += 180000; c.onCampaignHostReady();
assert.equal(c.requests.length, 0); assert.equal(c.errors.length, 1); assert(c.errors[0].includes('180 seconds')); assert(!watching(c));
c = production(); c.onChangeReadyStateHandle({});
dvars.ui_p2p_error = 'Waiting to publish the local host route.'; c.onCampaignHostReady();
assert.equal(c.errors.length, 0); assert(watching(c), 'transient publication keeps the production wait');
console.log('Production protocol 3 keeps its gate: Ready/Start withheld until the observed generation\'s initial handoff for the selected mission; duplicate, cancel, failure, 180 s timeout and stale callbacks fail closed');
