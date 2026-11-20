// Offline documentation export only. Never preprocess, compile, import game
// modules, run fixtures, or contact the reference server/client.
import { readFile, writeFile, mkdir, readdir, stat } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';

const wiki = fileURLToPath(new URL('..', import.meta.url));
const root = path.resolve(wiki, '..');
const slash = value => value.split(path.sep).join('/');
const read = relative => readFile(path.join(root, relative), 'utf8');
const digest = value => createHash('sha256').update(value).digest('hex');
async function walk(relative) {
  const files = [];
  for (const entry of await readdir(path.join(root, relative), { withFileTypes: true })) {
    const name = `${relative}/${entry.name}`;
    if (entry.isDirectory()) files.push(...await walk(name));
    else if (entry.isFile()) files.push(name); // Do not follow symlinks.
  }
  return files.sort();
}
async function exported(relative, content) {
  const output = path.join(wiki, 'public/repository', relative);
  await mkdir(path.dirname(output), { recursive: true });
  await writeFile(output, content, 'utf8');
}
const ledgerText = await read('docs/CODOL_IMPLEMENTATION_PASS.json');
const inventoryText = await read('docs/CODOL_BUILTIN_INVENTORY.json');
const ledger = JSON.parse(ledgerText);
const inventory = JSON.parse(inventoryText);
const docFiles = (await walk('docs')).filter(file => /\.(md|json)$/i.test(file));
docFiles.push('README.md', 'License.md', 'Copyright.md', 'copying.md');
const documents = [];
for (const file of docFiles) {
  const body = await read(file);
  const markdown = file.endsWith('.md');
  documents.push({ id: slash(file), title: markdown
    ? body.match(/^#\s+(.+)$/m)?.[1]?.trim() || path.basename(file)
    : path.basename(file, '.json').replace(/^CODOL_/, '').replaceAll('_', ' '),
    format: markdown ? 'markdown' : 'json', body,
    modified: (await stat(path.join(root, file))).mtime.toISOString(), sha256: digest(body) });
  await exported(file, body);
}

// Lexical evidence, intentionally NOT a preprocessed active-build inventory.
// Preserve offsets/newlines while hiding comments. Strings remain for names.
function uncomment(text) {
  return text.replace(/"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|\/\*[\s\S]*?\*\/|\/\/[^\r\n]*/g,
    token => token.startsWith('/') ? token.replace(/[^\r\n]/g, ' ') : token);
}
const sourceFiles = (await walk('src')).filter(file => /\.(c|cpp|h|asm|inc)$/i.test(file));
const records = new Map();
function record(name) {
  const key = name.toLowerCase();
  if (!records.has(key)) records.set(key, { name: key, registrations: [], definitions: [], events: [] });
  return records.get(key);
}
for (const entry of inventory.entries) record(entry.name).inventory = entry;
for (const entry of ledger.entries) record(entry.name).pass = entry;
const sources = [];
const sourceTexts = [];
for (const file of sourceFiles) {
  const body = await read(file);
  const clean = file.endsWith('.asm') ? body : uncomment(body);
  sourceTexts.push({ file, body, clean });
  if (!/\.(c|cpp)$/i.test(file)) continue;
  for (const hit of clean.matchAll(/\bScr_Add(Function|Method)\s*\(\s*"([^"]+)"\s*,\s*(\w+)\s*,\s*([^)]*)\)/g)) {
    record(hit[2]).registrations.push({ kind: hit[1].toLowerCase(), binding: hit[3],
      developerExpression: hit[4].trim(), file, line: clean.slice(0, hit.index).split('\n').length });
  }
}
const bindingOwners = new Map();
for (const entry of records.values()) {
  const symbols = new Set(entry.registrations.map(row => row.binding));
  for (const binding of Object.values(entry.inventory?.native || {})) symbols.add(binding.binding);
  for (const symbol of symbols) {
    if (!symbol) continue;
    if (!bindingOwners.has(symbol)) bindingOwners.set(symbol, []);
    bindingOwners.get(symbol).push(entry);
  }
}
// Locate ordinary C/C++ definition shapes or assembly labels, without claiming
// compiler-level symbol resolution. Macro-generated definitions can be absent.
for (const { file, body, clean } of sourceTexts) {
  let referenced = false;
  const pattern = file.endsWith('.asm') ? /^([A-Za-z_]\w*):\s*$/gm
    : /\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{/g;
  for (const hit of clean.matchAll(pattern)) {
    const owners = bindingOwners.get(hit[1]);
    if (!owners) continue;
    const line = clean.slice(0, hit.index).split('\n').length;
    for (const owner of owners) owner.definitions.push({ symbol: hit[1], file, line });
    referenced = true;
  }
  if (referenced || file.startsWith('src/codol/') || file === 'src/scr_vm_main.c') {
    sources.push({ path: file, lines: body.split('\n').length, sha256: digest(body),
      area: file.startsWith('src/codol/') ? file.split('/')[2] : 'retained engine' });
    await exported(file, body);
  }
}
// Export all registration files too, even if no ordinary definition was found.
for (const file of new Set([...records.values()].flatMap(e => e.registrations.map(r => r.file)))) {
  if (sources.some(s => s.path === file)) continue;
  const body = await read(file);
  sources.push({ path: file, lines: body.split('\n').length, sha256: digest(body), area: 'retained engine' });
  await exported(file, body);
}
const events = Object.entries(ledger).filter(([key, value]) => key !== 'entries' && value && typeof value === 'object')
  .map(([id, fields]) => ({ id, title: id.replaceAll('_', ' '), fields }));
const indexedEvents = events.map(event => ({ id: event.id,
  words: new Set((event.id.replaceAll('_', ' ') + ' ' + JSON.stringify(event.fields)).toLowerCase().match(/[a-z][a-z0-9_]*/g) || []) }));
for (const entry of records.values()) {
  entry.events = indexedEvents.filter(event => event.words.has(entry.name)).map(event => event.id);
}
const data = { generatedAt: new Date().toISOString(),
  provenance: { ledgerSha256: digest(ledgerText), inventorySha256: digest(inventoryText),
    registryMode: 'Lexical source scan; comments excluded; conditional branches not evaluated. Not active-build or semantic coverage.',
    eventMatching: 'Exact name-token mentions only; related records may be missing or incidental. Source order is preserved; no chronology inferred.' },
  policy: ledger.validation_policy, scope: ledger.scope,
  documents, events, functions: [...records.values()].sort((a, b) => a.name.localeCompare(b.name)),
  sources: sources.sort((a, b) => a.path.localeCompare(b.path)) };
await mkdir(path.join(wiki, 'src/generated'), { recursive: true });
await writeFile(path.join(wiki, 'src/generated/content.json'), JSON.stringify(data), 'utf8');
console.log(`Exported ${documents.length} documents, ${data.functions.length} function names, ${events.length} ledger records and ${sources.length} source files. No build or runtime validation performed.`);
