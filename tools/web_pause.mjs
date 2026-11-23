#!/usr/bin/env node
// Pause a busy page's JS/wasm via the debugger and print the stack (for spins).
//   node tools/web_pause.mjs [--cdp 9334] [--close]
const cdp = Number(process.argv.includes('--cdp') ? process.argv[process.argv.indexOf('--cdp') + 1] : 9334);
const tabs = await (await fetch(`http://127.0.0.1:${cdp}/json`)).json();
const tab = tabs.find((t) => t.type === 'page' && t.url.includes('iso='));
if (!tab) { console.log('no game tab'); process.exit(0); }
const ws = new WebSocket(tab.webSocketDebuggerUrl);
await new Promise((r) => ws.addEventListener('open', r, { once: true }));
let id = 1; const pending = new Map();
ws.addEventListener('message', (ev) => {
  const m = JSON.parse(ev.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); }
  if (m.method === 'Debugger.paused') {
    for (const f of m.params.callFrames.slice(0, 25))
      console.log(`  ${f.functionName || '?'}  @${f.location.columnNumber?.toString(16) ?? ''}`);
    send('Debugger.resume').then(async () => {
      if (process.argv.includes('--close')) await fetch(`http://127.0.0.1:${cdp}/json/close/${tab.id}`);
      process.exit(0);
    });
  }
});
const send = (method, params = {}) => new Promise((r) => { const i = id++; pending.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
console.log('connected'); send('Debugger.enable').then(() => console.log('enabled'));
send('Debugger.pause').then(() => console.log('pause sent'));
setTimeout(() => { console.log("pause timed out"); process.exit(1); }, 110000);
