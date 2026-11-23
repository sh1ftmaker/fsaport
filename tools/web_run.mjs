#!/usr/bin/env node
// Drive the page in a Chromium you started with --remote-debugging-port (see
// docs/PORTING.md, "Running in Chrome"): open a tab, mirror its console, take
// screenshots, send key presses, then close the tab.
//
//   node tools/web_run.mjs --cdp 9334 --url 'http://127.0.0.1:8790/?iso=local.iso' \
//        --seconds 60 --every 10 --out build/web-run [--keys 'Enter@30,KeyX@40']
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
await send('Page.navigate', { url });
const t0 = Date.now();
let shot = 0;
const evalJs = async (expr) => (await send('Runtime.evaluate', { expression: expr, returnByValue: true })).result?.result?.value;
const keyOf = (code) => {
  const key = code.startsWith('Key') ? code.slice(3).toLowerCase() : code;
  const vk = { Enter: 13, ArrowUp: 38, ArrowDown: 40, ArrowLeft: 37, ArrowRight: 39, Space: 32 }[code] ?? key.toUpperCase().charCodeAt(0);
  return { key, code, windowsVirtualKeyCode: vk };
};
while (Date.now() - t0 < seconds * 1000) {
  await sleep(250);
  const t = (Date.now() - t0) / 1000;
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
  if (t >= (shot + 1) * every) {
    shot++;
    const r = await send('Page.captureScreenshot', { format: 'png' });
    if (r.result) fs.writeFileSync(path.join(out, `shot-${String(shot * every).padStart(3, '0')}s.png`), Buffer.from(r.result.data, 'base64'));
    const stats = await evalJs('JSON.stringify(window.fsaStats ? window.fsaStats() : null)');
    if (stats && stats !== 'null') log(`[stats @${Math.round(t)}s] ${stats}`);
  }
}
await fetch(`http://127.0.0.1:${cdp}/json/close/${tab.id}`);
ws.close();
log('[done]');
process.exit(0);
