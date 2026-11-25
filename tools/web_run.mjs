#!/usr/bin/env node
// Drive the page in a Chromium you started with --remote-debugging-port (see
// docs/PORTING.md, "Running in Chrome"): open a tab, mirror its console, take
// screenshots, send key presses, then close the tab.
//
//   node tools/web_run.mjs --cdp 9334 --url 'http://127.0.0.1:8790/?iso=local.iso' \
//        --seconds 60 --every 10 --out build/web-run [--keys 'Enter@30,KeyX@40'] [--freeze 40:20]
//        [--evals 'Module._fsa_dump_threads()@70;...'] (JS run in the page at a time)
//        [--wav FILE]  (with ?audiorec=S in the URL: save the game's sound as a WAV)
//        [--until-script [--after S]]  (stop early when the ?script= run ends)
import fs from 'node:fs';
import path from 'node:path';

const args = {};
for (let i = 2; i < process.argv.length; i++) {
  const a = process.argv[i];
  if (a.startsWith('--')) args[a.slice(2)] = process.argv[i + 1] && !process.argv[i + 1].startsWith('--') ? process.argv[++i] : 'true';
}
const cdp = Number(args.cdp || 9334);
const url = args.url || 'http://127.0.0.1:8790/?iso=local.iso';
const seconds = Number(args.seconds || 60);
const every = Number(args.every || 10);
const out = args.out || 'build/web-run';
fs.mkdirSync(out, { recursive: true });
const logFile = fs.createWriteStream(path.join(out, 'console.log'));
const log = (s) => { logFile.write(s + '\n'); if (!args.quiet) console.log(s); };
const keys = (args.keys && args.keys !== 'true' ? args.keys.split(',') : []).map((s) => {
  const [code, t] = s.split('@');
  return { code, t: Number(t) };
});

const tab = await (await fetch(`http://127.0.0.1:${cdp}/json/new?about:blank`, { method: 'PUT' })).json();
const ws = new WebSocket(tab.webSocketDebuggerUrl);
await new Promise((r) => ws.addEventListener('open', r, { once: true }));
let nextId = 1;
const pending = new Map();
ws.addEventListener('message', (ev) => {
  const m = JSON.parse(ev.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); return; }
  if (m.method === 'Runtime.consoleAPICalled')
    log(`[${m.params.type}] ` + m.params.args.map((a) => a.value ?? a.description ?? '').join(' '));
  else if (m.method === 'Runtime.exceptionThrown')
    log('[exception] ' + (m.params.exceptionDetails.exception?.description || m.params.exceptionDetails.text));
  else if (m.method === 'Log.entryAdded')
    log(`[log.${m.params.entry.level}] ${m.params.entry.text}`);
});
const send = (method, params = {}) => new Promise((resolve) => {
  const id = nextId++;
  pending.set(id, resolve);
  ws.send(JSON.stringify({ id, method, params }));
});
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

await send('Runtime.enable');
await send('Log.enable');
await send('Page.enable');
if (args.ua) await send('Emulation.setUserAgentOverride', { userAgent: args.ua });
await send('Page.navigate', { url });
const t0 = Date.now();
let shot = 0;
const evalJs = async (expr) => (await send('Runtime.evaluate', { expression: expr, returnByValue: true })).result?.result?.value;
const keyOf = (code) => {
  const key = code.startsWith('Key') ? code.slice(3).toLowerCase() : code;
  const vk = { Enter: 13, ArrowUp: 38, ArrowDown: 40, ArrowLeft: 37, ArrowRight: 39, Space: 32 }[code] ?? key.toUpperCase().charCodeAt(0);
  return { key, code, windowsVirtualKeyCode: vk };
};
const evals = (args.evals && args.evals !== 'true' ? args.evals.split(';') : []).map((s) => {
  const at = s.lastIndexOf('@');
  return { expr: s.slice(0, at), t: Number(s.slice(at + 1)) };
});
// --throttle LATENCY_MS:KBPS emulates a slow link (the proxy, a phone).
if (args.throttle && args.throttle !== 'true') {
  const [lat, kbps] = args.throttle.split(':').map(Number);
  await send('Network.enable', {});
  await send('Network.emulateNetworkConditions', { offline: false, latency: lat,
    downloadThroughput: kbps * 128, uploadThroughput: kbps * 128 });
  log(`[throttle] ${lat} ms, ${kbps} kbit/s`);
}
// --freeze T:D freezes the page at T s (as Chrome does to a background tab)
// and wakes it D s later.
const freeze = args.freeze && args.freeze !== 'true' ? args.freeze.split(':').map(Number) : null;
let frozen = 0;
// --until-script: end the run (before --seconds) once a harness script
// (?script=) has written script_end or script_timeout to its sync stream;
// the page's Module.fsaDiag is wrapped to see it. --after S waits S s more.
const untilScript = args['until-script'] === 'true';
const after = Number(args.after || 0);
const hookScript = `(() => { if (typeof Module === 'undefined' || !Module.fsaDiag) return null;
  if (!Module.fsaDiag.__untilHook) { const o = Module.fsaDiag;
    Module.fsaDiag = (n, l) => { if (n === 'sync' && /"sync":"script_(end|timeout)"/.test(l)) window.__fsaScriptDone = l; return o(n, l); };
    Module.fsaDiag.__untilHook = 1; }
  return window.__fsaScriptDone || null; })()`;
let scriptDoneAt = 0, lastPoll = 0;
while (Date.now() - t0 < seconds * 1000) {
  await sleep(250);
  const t = (Date.now() - t0) / 1000;
  if (untilScript && t - lastPoll >= 1) {
    lastPoll = t;
    const done = scriptDoneAt ? null : await evalJs(hookScript);
    if (done) { scriptDoneAt = t; log(`[until-script] ${done} at ${t.toFixed(1)} s`); }
    if (scriptDoneAt && t >= scriptDoneAt + after) break;
  }
  for (const k of keys) {
    if (!k.done && t >= k.t) {
      k.done = true;
      await evalJs("document.getElementById('canvas').focus()");
      await send('Input.dispatchKeyEvent', { type: 'keyDown', ...keyOf(k.code) });
      await sleep(150);
      await send('Input.dispatchKeyEvent', { type: 'keyUp', ...keyOf(k.code) });
      log(`[key] ${k.code} at ${t.toFixed(1)} s`);
    }
  }
  if (freeze && frozen === 0 && t >= freeze[0]) {
    frozen = 1;
    await send('Page.setWebLifecycleState', { state: 'frozen' });
    log(`[freeze] page frozen at ${t.toFixed(1)} s`);
  } else if (freeze && frozen === 1 && t >= freeze[0] + freeze[1]) {
    frozen = 2;
    await send('Page.setWebLifecycleState', { state: 'active' });
    log(`[freeze] page woken at ${t.toFixed(1)} s`);
  }
  for (const e of evals) {
    if (!e.done && t >= e.t) {
      e.done = true;
      log(`[eval @${t.toFixed(1)}s] ${e.expr} -> ${JSON.stringify(await evalJs(e.expr))}`);
    }
  }
  if (t >= (shot + 1) * every) {
    shot++;
    const r = await send('Page.captureScreenshot', { format: 'png' });
    if (r.result) fs.writeFileSync(path.join(out, `shot-${String(shot * every).padStart(3, '0')}s.png`), Buffer.from(r.result.data, 'base64'));
    const stats = await evalJs('JSON.stringify(window.fsaStats ? window.fsaStats() : null)');
    if (stats && stats !== 'null') log(`[stats @${Math.round(t)}s] ${stats}`);
  }
}
// --wav FILE: save the sound the game sent (open the page with ?audiorec=S).
if (args.wav && args.wav !== 'true') {
  const parts = [];
  for (let off = 0; ; off += 1 << 20) {
    const b64 = await evalJs(`Module.fsaAudio.recChunk ? Module.fsaAudio.recChunk(${off}, ${1 << 20}) : ''`);
    if (!b64) break;
    parts.push(Buffer.from(b64, 'base64'));
  }
  const pcm = Buffer.concat(parts);
  const h = Buffer.alloc(44);
  h.write('RIFF', 0); h.writeUInt32LE(36 + pcm.length, 4); h.write('WAVEfmt ', 8);
  h.writeUInt32LE(16, 16); h.writeUInt16LE(1, 20); h.writeUInt16LE(2, 22); h.writeUInt32LE(32000, 24);
  h.writeUInt32LE(32000 * 4, 28); h.writeUInt16LE(4, 32); h.writeUInt16LE(16, 34);
  h.write('data', 36); h.writeUInt32LE(pcm.length, 40);
  fs.writeFileSync(args.wav, Buffer.concat([h, pcm]));
  log(`[wav] ${args.wav}: ${(pcm.length / 128000).toFixed(1)} s`);
}
// Diagnostics streams (docs/DIAGNOSTICS.md): write out the counters and post
// what is buffered before the tab goes.
if (await evalJs('typeof window.fsaDiagFlush') === 'function') {
  await evalJs('window.fsaDiagFlush()');
  // The coverage build posts a few MB (its counters); give it time.
  await sleep(await evalJs('typeof Module._fsa_cov_dump') === 'function' ? 6000 : 1500);
}
await fetch(`http://127.0.0.1:${cdp}/json/close/${tab.id}`);
ws.close();
log('[done]');
process.exit(0);
