// Execute the actual production GSC XP policy bodies. Engine I/O and the
// GSC int conversion are fixtures; native dvar conversion has its own C test.
const fs = require('fs');
const vm = require('vm');
const assert = require('assert/strict');
const path = require('path');
const root = path.join(__dirname, '..');
const rank = fs.readFileSync(path.join(root, 'bin/home/codol_rawfiles/maps/mp/gametypes/_rank.gsc'), 'utf8');
const damage = fs.readFileSync(path.join(root, 'bin/home/codol_rawfiles/maps/mp/gametypes/_damage.gsc'), 'utf8');
function body(name, source = rank) {
  const start = new RegExp('^' + name + '\\([^\\n]*\\)\\s*\\{', 'm').exec(source);
  assert(start, name);
  let offset = start.index + start[0].length, end = offset, depth = 1;
  while (depth) { if (source[end] === '{') depth++; if (source[end] === '}') depth--; end++; }
  return source.slice(offset, end - 1).replace(/\/\/[^\n]*/g, '');
}
const scores = {kill: 50, headshot: 50, assist: 10, capture: 150};
const context = {int: value => Math.trunc(value) + 0, getScoreInfoValue: type => scores[type]};
vm.createContext(context);
for (const [name, args, locals] of [
  ['getRankXPValue', 'type', ''], ['getKillRankXP', 'isHeadshot,inLastStand', 'let value;'],
  ['scaleRankXP', 'value,scale', 'let scaledValue;']]) {
  vm.runInContext(`function ${name}(${args}) { ${locals} ${body(name)} }`, context);
}
assert.equal(context.getKillRankXP(false, false), 100);
assert.equal(context.getKillRankXP(true, false), 125);
assert.equal(context.getKillRankXP(false, true), 100);
assert.equal(context.getKillRankXP(true, true), 125);
assert.equal(context.getRankXPValue('assist'), 25);
assert.equal(context.getRankXPValue('capture'), 150);
assert.deepEqual(scores, {kill: 50, headshot: 50, assist: 10, capture: 150});
for (const [rate, kill, headshot, assist] of [[1, 100, 125, 25], [0.2, 20, 25, 5], [0.25, 25, 31, 6], [0.5, 50, 62, 12]]) {
  assert.equal(context.scaleRankXP(context.getKillRankXP(false, false), rate), kill);
  assert.equal(context.scaleRankXP(context.getKillRankXP(true, false), rate), headshot);
  assert.equal(context.scaleRankXP(context.getRankXPValue('assist'), rate), assist);
}
assert.equal(context.scaleRankXP(25, 0.0001), 1);
assert.equal(context.scaleRankXP(0, 0.0001), 0);
assert.equal(context.scaleRankXP(-25, 0.0001), 0);
assert.equal(context.scaleRankXP(125, 0), 0);
assert.equal(context.scaleRankXP(100, 4), 400);
// Integration assertions: these policy bodies are the live award producers,
// and the existing popup/profile gates consume their resulting amount.
assert.match(rank, /if \( !isDefined\( value \) \)\s*value = getRankXPValue\( type \);/);
assert.match(rank, /value = scaleRankXP\( value, level\.xpScale \);/);
assert.match(rank, /if \( !self rankingEnabled\(\) \)/);
assert.match(rank, /self thread scorePopup\( value, momentumBonus, color, 0 \);/);
assert.match(rank, /if \( level\.xpScale <= 0 \|\| !self rankingEnabled\(\) \)/);
assert.match(damage, /value = maps\\mp\\gametypes\\_rank::getKillRankXP\( sMeansOfDeath == "MOD_HEAD_SHOT", isDefined\( attacker\.lastStand \) \);/);
assert.match(damage, /giveRankXP\( "kill", value \)/);
assert.match(damage, /creditWeaponXP\( sWeapon, maps\\mp\\gametypes\\_rank::getScoreInfoValue\( "kill" \) \)/);
console.log('PASS: production XP policy, fixed kill/headshot amounts, 20%/25%/50% rates, minimum positive XP and score/weapon separation');

// Execute giveRankXP itself, not a reimplementation of its arithmetic. The
// real rested-XP, sync and popup bodies are included so hidden bonuses and
// popup accumulation cannot evade a helper-only test again. Only engine I/O,
// rank-cap transitions (these fixtures stay below the first transition), and
// the GSC scheduler are fixture boundaries.
let dvars, lives, activePopup, splashCount;
Object.assign(context, {
  min: Math.min, max: Math.max, isDefined: x => x !== undefined,
  isdefined: x => x !== undefined,
  getdvarint: name => Math.trunc(dvars[name] || 0),
  getDvarFloat: name => dvars[name] || 0,
  getGametypeNumLives: () => lives,
  rankShouldTrace: () => false, updateRank: () => false,
  println: () => {}, printLn: () => {}, splashNotify: () => splashCount++,
});

function translate(name, source) {
  return body(name, source)
    .replace(/self maps\\mp\\gametypes\\_aar::noteXP/g, 'self.noteXP')
    .replace(/thread maps\\mp\\gametypes\\_hud_message::splashNotify/g, 'splashNotify')
    .replace(/self\.hud_scorePopup thread maps\\mp\\gametypes\\_hud::fontPulse/g, 'self.hud_scorePopup.fontPulse')
    .replace(/\b(self(?:\.\w+)*)\s+(?:thread\s+)?(\w+)\s*\(/g, '$1.$2(')
    .replace(/&("[^"]*")/g, '$1')
    .replace(/\bwait\s*\(/g, 'yield (');
}
function compile(name, args, source, generator = false, exportName = name) {
  const code = translate(name, source);
  const params = new Set(args.split(',').filter(Boolean));
  const locals = [...new Set([
    ...[...code.matchAll(/^\s*(\w+)\s*[+*/-]?=(?!=)/gm)].map(m => m[1]),
    ...[...code.matchAll(/for\s*\(\s*(\w+)\s*=/g)].map(m => m[1]),
  ])].filter(name => !params.has(name));
  vm.runInContext(`function${generator ? '*' : ''} ${exportName}(${args}) { ${locals.length ? 'let ' + locals.join(',') + ';' : ''} ${code} }`, context);
}
function installPipeline(source) {
  compile('getRestXPAward', 'baseXP', source);
  compile('isLastRestXPAward', 'baseXP', source);
  compile('syncXPStat', '', source);
  compile('giveRankXP', 'type,value', source);
  compile('scorePopup', 'amount,bonus,hudColor,glowAlpha', source, true, 'scorePopupVM');
}
function fixture({rate = 0.5, rest = false, restScale = 0.5, xpScaler = 1, numLives = 0, ranked = true} = {}) {
  dvars = {scr_restxp_enable: +rest, scr_restxp_restedAwardScale: restScale};
  lives = numLives; activePopup = null; splashCount = 0;
  context.level = {xpScale: rate, gameType: 'combat', rankedMatch: ranked,
    teamBased: true, teamCount: {allies: 1, axis: 1}, hardcoreMode: false};
  const state = {profile: {restXPGoal: rest ? 100000 : 0}, commits: [], reports: [], displays: []};
  const self = {name: 'fixture', usingOnlineDataOffline: false, isbot: false,
    xpScaler, xpGains: {}, xpUpdateTotal: 0, bonusUpdateTotal: 0,
    pers: {rank: 0, rankxp: 1000, summary: {score: 0, xp: 0, match: 0, challenge: 0, misc: 0}},
    rankingEnabled: () => context.level.rankedMatch && !self.usingOnlineDataOffline,
    endon: () => {}, notify: name => { if (name === 'scorePopup' && activePopup) activePopup.return(); },
    getPlayerData: key => state.profile[key], setPlayerData: (key, value) => state.profile[key] = value,
    getRankXP: () => self.pers.rankxp, incRankXP: value => self.pers.rankxp += value,
    noteXP: (type, oldXP, newXP) => state.reports.push({type, amount: newXP - oldXP}),
    setTotalRankXP: value => state.commits.push(value), syncXPStat: () => context.syncXPStat(),
    setClientDvar: () => {}, creditRankUpGP: () => {}, updateRankAnnounceHUD: () => {},
    hud_scorePopup: {setValue: value => state.displays.push(value), fontPulse: () => {}, fadeOverTime: () => {}},
    scorePopup: (...args) => {
      const task = context.scorePopupVM(...args);
      if (!task.next().done) { activePopup = task; task.next(); }
    },
  };
  context.self = self;
  return {state, self};
}
function award(type, value, options, expected) {
  const {state, self} = fixture(options);
  context.giveRankXP(type, value);
  assert.equal(self.pers.rankxp - 1000, expected, 'actual account XP award');
  assert.equal(self.xpGains[type], expected, 'actual per-type XP accounting');
  assert.equal(self.pers.summary.xp, expected, 'actual end-of-match summary XP');
  assert.equal(state.reports[0].amount, expected, 'actual after-action report delta');
  assert.equal(state.commits.at(-1), 1000 + expected, 'actual syncXPStat profile value');
  assert.equal(state.displays.at(-1), expected, 'actual scorePopup HUD amount');
  return {state, self};
}
installPipeline(rank);
for (const [rate, kill, headshot, assist] of [[1, 100, 125, 25], [0.2, 20, 25, 5], [0.25, 25, 31, 6], [0.5, 50, 62, 12]]) {
  for (const modifiers of [{}, {rest: true}, {xpScaler: 2}, {numLives: 1}, {rest: true, xpScaler: 2, numLives: 1}]) {
    const options = {rate, ...modifiers};
    award('kill', context.getKillRankXP(false, false), options, kill);
    award('kill', context.getKillRankXP(true, false), options, headshot);
    award('headshot', undefined, options, assist); // Standalone explicit 25-XP bonus.
    award('assist', undefined, options, assist);
  }
}
award('kill', 100, {rate: 0.0001, rest: true, xpScaler: 2, numLives: 1}, 1);
let empty = fixture({rate: 0.2, rest: true});
context.self.usingOnlineDataOffline = true;
context.giveRankXP('kill', 100);
assert.equal(empty.self.pers.rankxp, 1000);
assert.equal(empty.state.commits.length, 0);
assert.equal(empty.state.displays.length, 0);
empty = fixture({rate: 0.2, ranked: false});
context.giveRankXP('kill', 100);
assert.equal(empty.self.pers.rankxp, 1000);
assert.equal(empty.state.commits.length, 0);
assert.equal(empty.state.displays.length, 0);
empty = fixture({rate: 0, rest: true});
context.giveRankXP('kill', 100);
assert.equal(empty.self.pers.rankxp, 1000);
assert.equal(empty.state.commits.length, 0);
assert.equal(empty.state.displays.length, 0);
const popup = fixture();
context.self.scorePopup(25, 0, 0, 0);
context.self.scorePopup(50, 0, 0, 0);
assert.deepEqual(popup.state.displays, [25, 50], 'a new popup shows its own award instead of prior-event accumulation');
assert.equal(popup.self.xpUpdateTotal, 50);
activePopup.next();
assert.equal(popup.self.xpUpdateTotal, 0, 'popup completion clears its display state');
console.log('PASS: actual giveRankXP, rested-XP/sync/popup bodies preserve advertised account/report/HUD amounts despite hidden modifiers; individual popups and zero/offline gates');

// Optional forensic red/green proof against a separately saved, unmodified
// pre-correction rank script; the tested production source stays untouched.
const legacyIndex = process.argv.indexOf('--legacy-rank');
if (legacyIndex >= 0) {
  const legacy = fs.readFileSync(process.argv[legacyIndex + 1], 'utf8');
  installPipeline(legacy);
  const before = fixture({rate: 0.5, rest: true, restScale: 0.5});
  context.giveRankXP('kill', 100);
  assert.equal(before.self.pers.rankxp - 1000, 75);
  assert.equal(before.state.displays.at(-1), 75);
  const beforePopup = fixture();
  context.self.scorePopup(25, 0, 0, 0);
  context.self.scorePopup(50, 0, 0, 0);
  assert.deepEqual(beforePopup.state.displays, [25, 75]);
  console.log('PASS: unmodified pre-correction bodies reproduce 75 from a 50% rested bonus or recent 25+50 popup accumulation (fixture evidence, not live-session attribution)');
}
