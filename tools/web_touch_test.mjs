#!/usr/bin/env node
// Drive the page as a phone over CDP: device emulation with touch, then a
// script of taps / drags on the on-screen controller (by element id), with
// screenshots. Mirrors the console like web_run.mjs.
//
//   node tools/web_touch_test.mjs --cdp 9334 --url 'http://127.0.0.1:8811/?iso=local.iso' \
//        --out build/web-run/touch --script 'portrait@0,shot@20,tap:tb-start@26,...'
//
// Steps: portrait / landscape (switch device metrics), shot (screenshot),
// tap:<id> (press and release the centre of an element), hold:<id>:<ms>,
// stick:<dx>,<dy>:<ms> (drag the stick from its centre by a fraction of its radius),
// press:<dx>,<dy> or press:<id> (hold without releasing), release,
// eval:<js>.
import fs from 'node:fs';
import path from 'node:path';

const args = {};
for (let i = 2; i < process.argv.length; i++) {
  const a = process.argv[i];
  if (a.startsWith('--')) args[a.slice(2)] = process.argv[++i];
}
const cdp = Number(args.cdp || 9334);
const out = args.out || 'build/web-run/touch';
fs.mkdirSync(out, { recursive: true });
const logFile = fs.createWriteStream(path.join(out, 'console.log'));
const log = (s) => logFile.write(s + '\n');
const steps = (args.script || '').split(';').filter(Boolean).map((s) => {
  const at = s.lastIndexOf('@');
  return { what: s.slice(0, at), t: Number(s.slice(at + 1)) };
});
const end = Number(args.seconds || (steps.length ? steps[steps.length - 1].t + 2 : 30));

const tab = await (await fetch(`http://127.0.0.1:${cdp}/json/new?about:blank`, { method: 'PUT' })).json();
const ws = new WebSocket(tab.webSocketDebuggerUrl);
await new Promise((r) => ws.addEventListener('open', r, { once: true }));
let nextId = 1;
const pending = new Map();
ws.addEventListener('message', (ev) => {
  const m = JSON.parse(ev.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); return; }
  if (m.method === 'Runtime.consoleAPICalled') log(`[${m.params.type}] ` + m.params.args.map((a) => a.value ?? a.description ?? '').join(' '));
  else if (m.method === 'Runtime.exceptionThrown') log('[exception] ' + (m.params.exceptionDetails.exception?.description || m.params.exceptionDetails.text));
});
const send = (method, params = {}) => new Promise((resolve) => {
  const id = nextId++;
  pending.set(id, resolve);
  ws.send(JSON.stringify({ id, method, params }));
});
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const evalJs = async (expr) => (await send('Runtime.evaluate', { expression: expr, returnByValue: true, awaitPromise: true })).result?.result?.value;

async function device(portrait) {
  const [w, h] = portrait ? [390, 844] : [844, 390];
  await send('Emulation.setDeviceMetricsOverride', {
    width: w, height: h, deviceScaleFactor: 2, mobile: true,
    screenOrientation: { type: portrait ? 'portraitPrimary' : 'landscapePrimary', angle: portrait ? 0 : 90 },
  });
  await send('Emulation.setTouchEmulationEnabled', { enabled: true, maxTouchPoints: 5 });
}
async function centre(id) {
  return evalJs(`(() => { const r = document.getElementById(${JSON.stringify(id)}).getBoundingClientRect();
    return { x: r.left + r.width / 2, y: r.top + r.height / 2, r: r.width / 2 }; })()`);
}
const touch = (type, pts) => send('Input.dispatchTouchEvent', { type, touchPoints: pts });

await send('Runtime.enable');
await send('Page.enable');
await device(true);
await send('Page.navigate', { url: args.url || 'http://127.0.0.1:8811/?iso=local.iso' });
const t0 = Date.now();
let shots = 0;
for (const s of steps) {
  const wait = s.t * 1000 - (Date.now() - t0);
  if (wait > 0) await sleep(wait);
  const [kind, ...rest] = s.what.split(':');
  const arg = rest.join(':');
  log(`[step @${((Date.now() - t0) / 1000).toFixed(1)}s] ${s.what}`);
  if (kind === 'portrait' || kind === 'landscape') await device(kind === 'portrait');
  else if (kind === 'shot') {
    const r = await send('Page.captureScreenshot', { format: 'png' });
    fs.writeFileSync(path.join(out, `shot-${String(++shots).padStart(2, '0')}-${Math.round(s.t)}s.png`), Buffer.from(r.result.data, 'base64'));
  } else if (kind === 'tap' || kind === 'hold') {
    const [id, ms] = arg.split(':');
    const c = await centre(id);
    await touch('touchStart', [{ x: c.x, y: c.y, id: 1 }]);
    await sleep(Number(ms || 120));
    await touch('touchEnd', []);
  } else if (kind === 'stick') {
    const [d, ms] = arg.split(':');
    const [dx, dy] = d.split(',').map(Number);
    const c = await centre('stick');
    await touch('touchStart', [{ x: c.x, y: c.y, id: 2 }]);
    for (let k = 1; k <= 5; k++) {
      await sleep(16);
      await touch('touchMove', [{ x: c.x + dx * c.r * k / 5, y: c.y + dy * c.r * k / 5, id: 2 }]);
    }
    await sleep(Number(ms || 500));
    await touch('touchEnd', []);
  } else if (kind === 'press') {        // press:<dx>,<dy> holds the stick (and press:A-style ids hold a button)
    if (arg.includes(',')) {
      const [dx, dy] = arg.split(',').map(Number);
      const c = await centre('stick');
      await touch('touchStart', [{ x: c.x, y: c.y, id: 2 }]);
      for (let k = 1; k <= 5; k++) {
        await sleep(16);
        await touch('touchMove', [{ x: c.x + dx * c.r * k / 5, y: c.y + dy * c.r * k / 5, id: 2 }]);
      }
    } else {
      const c = await centre(arg);
      await touch('touchStart', [{ x: c.x, y: c.y, id: 3 }]);
    }
  } else if (kind === 'release') {
    await touch('touchEnd', []);
  } else if (kind === 'eval') {
    log(`[eval] ${JSON.stringify(await evalJs(arg))}`);
  }
}
const left = end * 1000 - (Date.now() - t0);
if (left > 0) await sleep(left);
await fetch(`http://127.0.0.1:${cdp}/json/close/${tab.id}`);
ws.close();
log('[done]');
process.exit(0);
