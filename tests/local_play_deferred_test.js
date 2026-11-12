// Deferred Local Play host start (docs/P2P_DEFERRED_HOST_START.md): execute the actual ActionScript bodies of the
// room controller's Ready/Start routing and of the room helper's read-only status. Only the native/timer/display
// boundaries are doubled. Ready and Start must proceed at once in protocols 0-2 and never wait for a running host.
const fs = require('fs'), path = require('path'), assert = require('assert');
const asRoot = process.env.P2P_AS_ROOT || path.join(__dirname, '../zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as');
function reader(file) {
  const source = fs.readFileSync(path.join(asRoot, file), 'utf8');
  return name => {
    const start = source.indexOf('function ' + name + '(');
    assert(start >= 0, name + ' exists');
    const open = source.indexOf('{', start);
    let depth = 1, end = open + 1;
    while (depth && end < source.length) { if (source[end] === '{') depth++; if (source[end] === '}') depth--; end++; }
    return source.slice(open + 1, end - 1).replace(/var (\w+):[A-Za-z0-9_.]+/g, 'var $1');
  };
}
let now = 1000, dvars = {}, commands = [];
global.ScaleformUtil = {isScaleform: true};
global.MapType = {COOP: 'story'};
global.MsgBtnCount = {ONE: 1};
global.AsyncTaskID = {START_MATCH: 'start'};
global.int = x => Number(x) | 0;
global.CFunctionContainerCommon = {execnow: c => commands.push(c), Dvar_GetString: k => dvars[k] || '', setdvar: (k, v) => { dvars[k] = v; }};
global.getTimer = () => now;
global.TimerEvent = {TIMER: 'timer'};
global.Timer = class { addEventListener(e, cb) { this.callback = cb; } start() { this.running = true; } stop() { this.running = false; } };
global.LocalizeUtil = {localize: key => key === '@K33UI_LOCAL_PLAY' ? 'LOCAL PLAY' : key};

const lobby = reader('codo/controllers/LobbyController.as');
function controller(protocol) {
  now = 1000; commands = [];
  dvars = {ui_p2p_startup_protocol: protocol, ui_p2p_enabled: '1', ui_p2p_state: 'armed', ui_p2p_ready: '0', ui_p2p_armed: '1'};
  const c = {_campaignHostTimer: null, _campaignHostMap: '', _campaignHostRequestedMap: '', _campaignHostStartPara: null, _campaignHostSelectedMap: '',
    _campaignHostContext: '', _campaignHostRoomKey: '', _campaignHostGeneration: '', _localHostPendingAction: '', _campaignHostWaitStart: 0,
    owner: true, leaving: false, ready: false, requests: [], errors: [], readyStates: [],
    _roomObj: {isPlayList: false, mapType: 'mp', index: 7, name: 'edtest', currentMatchMode: {id: 'war'}, currentMap: {id: 'mp_killhouse_sh'}, currentLevel: 1},
    isRoomHost() { return this.owner; }, isLeavingRoom() { return this.leaving; }, isReady() { return this.ready; },
    request(id, args) { this.requests.push({id, args}); }, showMsg(m) { this.errors.push(m); },
    applyRoomReadyState(v) { this.ready = v; this.readyStates.push(v); }};
  const args = {beginDeferredLocalHost: 'startPara,action', beginCampaignLocalHost: 'startPara,waitForStart,action'};
  for (const name of ['isLocalCampaignRoom', 'localHostCategory', 'localHostGameType', 'isLocalHostRoom', 'localIntentRoomKey', 'isProductionLocalRoom',
    'usesLocalStartupProtocol', 'localHostContext', 'cancelCampaignHostStart', 'completeCampaignHostWait', 'beginDeferredLocalHost', 'onCampaignHostReady'])
    c[name] = new Function(args[name] || 'evt', lobby(name));
  return c;
}

// Protocol 0: Ready proceeds at once; the room context is (re)published; nothing waits for ui_p2p_ready.
let c = controller('0');
c.beginDeferredLocalHost(null, 'ready');
assert.deepEqual(commands, ['p2p_room mp_killhouse_sh war mp'], 'legacy Ready only republishes the room context');
assert.deepEqual(c.readyStates, [true], 'Ready is applied immediately, with no host running');
assert.equal(c.requests.length, 0);
assert.equal(c._localHostPendingAction, 'launch'); assert(c._campaignHostTimer.running, 'only a failure watch runs');
c.onCampaignHostReady(); assert.deepEqual(c.readyStates, [true]); assert.equal(c.errors.length, 0, 'an armed, not ready host is not an error');
dvars.ui_p2p_launching = '1'; dvars.ui_p2p_state = 'starting'; c.onCampaignHostReady(); assert(c._campaignHostTimer.running);
dvars.ui_p2p_ready = '1'; dvars.ui_p2p_state = 'ready'; c.onCampaignHostReady();
assert(!c._campaignHostTimer.running && c._localHostPendingAction === '', 'the watch ends when the host is ready');
assert.deepEqual(c.readyStates, [true], 'readiness never re-applies Ready');

// Start: the START_MATCH task is issued at once with its own parameters.
c = controller('0'); const parameters = {selectedDifficulty: 2};
c.beginDeferredLocalHost(parameters, 'start');
assert.deepEqual(c.requests, [{id: 'start', args: parameters}]); assert.deepEqual(c.readyStates, []);

// A start that failed is reported once and Ready is taken back; nothing falls through to another server.
c = controller('0'); c.beginDeferredLocalHost(null, 'ready');
dvars.ui_p2p_state = 'failed'; dvars.ui_p2p_error = 'the UDP port is in use'; c.onCampaignHostReady();
assert.deepEqual(c.readyStates, [true, false]); assert.equal(c.errors.length, 1); assert(c.errors[0].includes('the UDP port is in use'));
assert(!c._campaignHostTimer.running); c.onCampaignHostReady(); assert.equal(c.errors.length, 1);
c = controller('0'); c.beginDeferredLocalHost(null, 'ready');
dvars.ui_p2p_error = 'Waiting to publish the local host route.'; c.onCampaignHostReady();
assert.equal(c.errors.length, 0, 'a transient publication retry is not a failure');

// Leaving, losing the host seat, a changed map or an unticked choice revoke the pending launch silently.
for (const revoke of [x => { x.leaving = true; }, x => { x.owner = false; }, x => { x._roomObj.currentMap.id = 'mp_rust'; }, () => { dvars.ui_p2p_enabled = '0'; }]) {
  for (const protocol of ['0', '1']) {
    c = controller(protocol); c.beginDeferredLocalHost(null, 'ready'); commands = []; revoke(c); c.onCampaignHostReady();
    assert(!c._campaignHostTimer.running); assert.equal(c.errors.length, 0);
    assert.deepEqual(commands, protocol === '1' && dvars.ui_p2p_enabled === '1' ? ['p2p_cancel_start'] : []);
  }
}

// Protocols 1 and 2: the intent is sent (it only permits the launch natively) and the flow still proceeds at once.
c = controller('1'); c.beginDeferredLocalHost(null, 'ready');
assert.deepEqual(commands, ['p2p_start_intent ready mp_killhouse_sh war mp']); assert.deepEqual(c.readyStates, [true]);
c = controller('2'); c._roomObj.currentMap.id = 'zm_factory_sh'; c._roomObj.currentMatchMode.id = 'zm'; c._roomObj.currentLevel = 3;
c.beginDeferredLocalHost(parameters, 'start');
assert.deepEqual(commands, ['p2p_start_intent start zm_factory_sh zm zm 1 3']); assert.equal(c.requests.length, 1);
c = controller('2'); c._roomObj.currentMap.id = 'zm_factory_sh'; c._roomObj.currentMatchMode.id = 'zm'; c._roomObj.currentLevel = 9;
c.beginDeferredLocalHost(null, 'ready');
assert.deepEqual(c.readyStates, [], 'a malformed tier never becomes Ready');
assert.deepEqual(commands, ['p2p_cancel_start']);

// The two handlers route protocols 0-2 to the deferred path and keep the production protocol (3) on its SDK-sequenced gate.
const text = fs.readFileSync(path.join(asRoot, 'codo/controllers/LobbyController.as'), 'utf8');
assert.equal((text.match(/if\(this\.isProductionLocalRoom\(\)\) this\.beginCampaignLocalHost\(/g) || []).length, 2);
assert.equal((text.match(/else this\.beginDeferredLocalHost\(/g) || []).length, 2);
console.log('Deferred Local Play: Ready/Start proceed at once in protocols 0-2, failure and revocation paths, production gate untouched');

// The room helper: a read-only ticked box for the owner while the choice is enabled; launch and error captions.
const controls = reader('codo/k33/LocalMatchControls.as');
function helper(protocol) {
  dvars = {ui_p2p_startup_protocol: protocol, ui_p2p_enabled: '1', ui_p2p_state: 'armed'};
  const h = {room: {forces: {isHost: true}}, localSelection: true, visible: false, mouseEnabled: true, mouseChildren: true,
    choice: {selected: false, mouseEnabled: true, label: ''}};
  h.owner = new Function(controls('owner')); h.refreshStatus = new Function(controls('refreshStatus'));
  return h;
}
let h = helper('0'); h.refreshStatus();
assert(h.visible && h.choice.selected && !h.choice.mouseEnabled && !h.mouseChildren, 'armed choice is shown ticked and cannot be clicked');
assert.equal(h.choice.label, 'LOCAL PLAY');
dvars.ui_p2p_launching = '1'; h.refreshStatus(); assert.equal(h.choice.label, 'STARTING HOST...');
dvars.ui_p2p_error = 'the UDP port is in use'; h.refreshStatus(); assert.equal(h.choice.label, 'HOST ERROR');
dvars.ui_p2p_error = ''; dvars.ui_p2p_launching = '0'; dvars.ui_p2p_enabled = '0'; h.refreshStatus(); assert(!h.visible, 'unticked: no box');
h = helper('1'); h.room.forces.isHost = false; h.refreshStatus(); assert(!h.visible, 'guests see no box');
h = helper('2'); h.localSelection = false; h.refreshStatus(); assert(!h.visible);
h = helper('3'); h.visible = true; h.choice.mouseEnabled = false; h.refreshStatus();
assert(h.visible && h.choice.mouseEnabled && h.choice.label === '', 'the production checkbox stays clickable and untouched');
console.log('Room helper: read-only armed box, STARTING HOST and HOST ERROR captions, owner/enabled scope, production unchanged');
