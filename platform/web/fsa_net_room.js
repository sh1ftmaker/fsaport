// --pre-js (deluxe): netplay rooms, the page side (docs/NETPLAY.md): the relay
// connection, the lobby (create / join, seats, start), the input transport,
// pings, time sync, hashes, and what the player sees (lobby panel, status
// chip, a loud desync banner). The timeline and rollback are fsa_net.js.
//
//   ?room=new&seats=N    create a room (the relay picks the code; the URL is
//                        rewritten to ?room=CODE so it can be shared)
//   ?room=CODE           join it (?seat=N to ask for seat N, 1-based)
//   ?relay=URL           the relay (default: ws://127.0.0.1:8858/ locally, the
//                        terminal proxy's /lp/8858/ when the page is behind it)
//   ?name=NAME           shown in the seat list
//   room options (taken from the creator's URL, every browser uses them):
//   ?net=rollback|lockstep  ?delay=D  ?window=W  ?snapevery=K  ?script=URL
//   ?autostart=N         start as soon as N seats are taken (tests)
//   ?lat=MS&jitter=MS    artificial latency on this browser's relay link (tests)
Module.fsaNetLobby = async function () {
  const q = new URLSearchParams(location.search);
  const roomArg = q.get('room');
  if (!roomArg) return null;
  const log = (s) => (window.fsaLog || console.log)('[room] ' + s);
  const ui = netUi();

  // Which build and which disc: a room only takes browsers that agree on both.
  const sha = async (buf) => [...new Uint8Array(await crypto.subtle.digest('SHA-256', buf))].map((b) => b.toString(16).padStart(2, '0')).join('');
  ui.status('Checking the build and the disc...');
  const wasm = await (await fetch('fsa.wasm')).arrayBuffer();
  const build = (await sha(wasm)).slice(0, 16);
  const head = await Module.fsaDisc.read(0, 0x440);
  const discId = String.fromCharCode(...head.subarray(0, 6)) + '-' + head[7] + '-' + (await sha(await Module.fsaDisc.read(0, 1 << 20))).slice(0, 12);

  let relay = q.get('relay');
  if (!relay) {
    const m = /^\/lp\/\d+\//.exec(location.pathname);
    relay = m ? `wss://${location.host}/lp/8858/` : `ws://${location.hostname || '127.0.0.1'}:8858/`;
  }
  const params = new URLSearchParams();
  if (q.get('lat')) params.set('lat', q.get('lat'));
  if (q.get('jitter')) params.set('jitter', q.get('jitter'));
  const wsUrl = relay + (params.toString() ? '?' + params : '');
  const create = roomArg.toLowerCase() === 'new' || q.get('create') === '1';
  const opts = {
    seats: Number(q.get('seats') || 2), mode: q.get('net') || 'rollback',
    delay: q.get('delay') !== null ? Number(q.get('delay')) : null,
    window: q.get('window') !== null ? Number(q.get('window')) : null,
    snapevery: q.get('snapevery') !== null ? Number(q.get('snapevery')) : null,
    script: q.get('script') || null, autostart: Number(q.get('autostart') || 0),
  };
  const name = q.get('name') || (navigator.maxTouchPoints > 0 ? 'phone' : 'player');

  const sess = {
    room: roomArg.toUpperCase(), seat: -1, seats: 0, opts: null, left: [], build, disc: discId,
    rtt: 0, rttDev: 0, peers: new Map(), sent: 0, recv: 0, bytesOut: 0, bytesIn: 0, desyncs: [],
  };
  ui.status(`Connecting to the relay (${relay})...`);
  const ws = new WebSocket(wsUrl);
  ws.binaryType = 'arraybuffer';
  const send = (o) => { if (ws.readyState === 1) ws.send(typeof o === 'string' || o instanceof Uint8Array ? o : JSON.stringify(o)); };
  let core = null;  // fsa_net.js's state, once the game runs
  const early = [];  // inputs that arrive before the core is attached

  const started = new Promise((resolve, reject) => {
    ws.onopen = () => send({ t: 'hello', v: 1, room: roomArg.toLowerCase() === 'new' ? 'NEW' : sess.room, create, name, build, disc: discId, opts,
                             seat: q.get('seat') ? Number(q.get('seat')) - 1 : -1 });
    ws.onerror = () => reject(new Error('could not reach the relay at ' + relay));
    ws.onclose = () => {
      if (!sess.opts) reject(new Error('the relay closed the connection'));
      else { ui.banner('Disconnected from the relay: the other players now see this seat as idle.', true); if (core) core.dead = true; }
    };
    ws.onmessage = (ev) => {
      if (typeof ev.data !== 'string') { onInput(new Uint8Array(ev.data)); return; }
      const m = JSON.parse(ev.data);
      if (m.t === 'error') { reject(new Error(m.msg)); return; }
      if (m.t === 'welcome') {
        sess.room = m.room; sess.seat = m.seat; sess.opts = m.opts; sess.host = m.host;
        if (create || q.get('room') !== m.room) {
          const u = new URL(location.href);
          u.searchParams.set('room', m.room);
          for (const k of ['create', 'seats', 'net', 'delay', 'window', 'snapevery', 'autostart']) u.searchParams.delete(k);
          history.replaceState(null, '', u);
        }
        log(`room ${m.room}: seat ${m.seat + 1} of ${m.opts.seats} (${m.opts.mode})`);
        ui.lobby(sess, () => send({ t: 'start' }));
        return;
      }
      if (m.t === 'roster') { sess.roster = m.seats; ui.lobby(sess, () => send({ t: 'start' })); return; }
      if (m.t === 'start') {
        sess.seats = m.opts.seats; sess.opts = m.opts; sess.left = m.left || [];
        log(`start: seats ${m.seats.map((s) => s + 1).join(', ')}`);
        resolve();
        return;
      }
      if (m.t === 'pong') {
        const rtt = performance.now() - m.c;
        sess.rtt = sess.rtt ? sess.rtt * 0.8 + rtt * 0.2 : rtt;
        sess.rttDev = sess.rttDev * 0.8 + Math.abs(rtt - sess.rtt) * 0.2;
        return;
      }
      if (m.t === 'stat') { const p = sess.peers.get(m.seat) || {}; p.rtt = m.rtt; sess.peers.set(m.seat, p); return; }
      if (m.t === 'leave') { log(`seat ${m.seat + 1} left at frame ${m.at}`); if (core) core.seatLeft(m.seat, m.at); else sess.left.push(m); ui.toast(`Player ${m.seat + 1} left`); return; }
      if (m.t === 'desync') {
        sess.desyncs.push(m);
        const msg = `the browsers disagree at read ${m.n} (state hashes ${JSON.stringify(m.hashes)})`;
        if (core) core.fatal(msg);
        return;
      }
    };
  });
  setInterval(() => send({ t: 'ping', c: performance.now() }), 1000);
  send({ t: 'ping', c: performance.now() });
  try {
    await started;
  } catch (e) {
    ui.error(String(e.message || e));
    throw e;
  }
  ui.hideLobby();

  function onInput(b) {
    if (b[0] !== 0x01 || b.length < 12) return;
    const dv = new DataView(b.buffer, b.byteOffset, b.byteLength);
    const seat = b[1], first = dv.getUint32(2, true), count = dv.getUint16(6, true), head = dv.getUint32(8, true);
    const vals = [];
    for (let i = 0; i < count; i++) {
      const o = 12 + 9 * i;
      vals.push([dv.getUint32(o + 1, true), dv.getUint32(o + 5, true), dv.getInt8(o)]);
    }
    sess.recv += count; sess.bytesIn += b.length;
    const peer = sess.peers.get(seat) || {};
    peer.inHead = head; peer.inAt = performance.now();
    sess.peers.set(seat, peer);
    if (core) core.receive(seat, first, vals); else early.push([seat, first, vals]);
  }

  sess.attach = (S) => {
    core = S;
    for (const [seat, first, vals] of early) S.receive(seat, first, vals);
    early.length = 0;
    S.onLocalInputs = (port, first, vals, n) => {
      const b = new Uint8Array(12 + 9 * vals.length);
      const dv = new DataView(b.buffer);
      b[0] = 0x01; b[1] = port;
      dv.setUint32(2, first, true); dv.setUint16(6, vals.length, true); dv.setUint32(8, n, true);
      vals.forEach((v, i) => { const o = 12 + 9 * i; dv.setInt8(o, v[2]); dv.setUint32(o + 1, v[0], true); dv.setUint32(o + 5, v[1], true); });
      sess.sent += vals.length; sess.bytesOut += b.length;
      send(b);
    };
    S.onHash = (n, h) => send({ t: 'hash', n, h });
    S.onDesync = (msg) => ui.banner('OUT OF SYNC: ' + msg, true);
    setInterval(() => send({ t: 'stat', head: S.head, rtt: sess.rtt }), 500);
    // Frame advantage over the slowest other player (GGPO-style time sync):
    // its read count when it sent its latest input, plus the frames that input
    // took to get here (half of each link's round trip), against ours. When we
    // are more than a frame ahead, inputs() waits one frame (returns true).
    const FRAME = 1001 / 60;
    let adv = 0;
    S.timeSync = (n) => {
      let worst = -Infinity;
      const now = performance.now();
      for (const [seat, p] of sess.peers) {
        if (seat === sess.seat || p.inHead === undefined || core.left.has(seat)) continue;
        const est = p.inHead + (now - p.inAt + (p.rtt || sess.rtt) / 2 + sess.rtt / 2) / FRAME;
        worst = Math.max(worst, n - est);
      }
      if (worst === -Infinity) return false;
      adv = adv * 0.9 + worst * 0.1;
      if (adv > 1.0) { adv -= 1; return true; }
      return false;
    };
    // Lockstep: the delay that covers this link and the worst other one.
    S.adapt = () => {
      if (S.head % 30) return;
      let worst = 0;
      for (const [seat, p] of sess.peers) if (seat !== sess.seat && p.rtt) worst = Math.max(worst, p.rtt);
      const ms = sess.rtt / 2 + worst / 2 + 2 * sess.rttDev + 8;
      const want = Math.max(1, Math.min(30, Math.ceil(ms / FRAME) + 1));
      if (want > S.delay) S.delay++;
      else if (want < S.delay - 1) S.delay--;
    };
    S.onStall = (ms) => { if (ms > 250) ui.toast(`waiting for the others (${Math.round(ms)} ms)`); };
    sess.report = () => ({ rtt: Math.round(sess.rtt), rttDev: Math.round(sess.rttDev), sent: sess.sent, recv: sess.recv,
                           bytesOut: sess.bytesOut, bytesIn: sess.bytesIn, relayDesyncs: sess.desyncs.length });
    ui.chip(() => {
      const st = S.stats;
      return `room ${sess.room} · P${sess.seat + 1} · ${Math.round(sess.rtt)} ms · delay ${S.delay}` +
             (S.rollback ? ` · rb ${st.rollbacks}` : '') + ` · waits ${st.stalls}` + (st.desyncs ? ' · DESYNC' : '');
    });
  };
  Module.fsaNetSession = sess;
  window.fsaNetSession = sess;
  return sess;
};

// The page's netplay UI: a lobby panel, a status chip, toasts and a banner.
// Self-contained (its own DOM and styles), so it does not touch the page's.
function netUi() {
  if (window.fsaNetUi) return window.fsaNetUi;
  const css = document.createElement('style');
  css.textContent = `
  #fsa-net-lobby{position:fixed;inset:0;display:flex;align-items:center;justify-content:center;background:rgba(0,0,0,.6);z-index:50;font:15px/1.4 system-ui,sans-serif;color:#eee}
  #fsa-net-lobby .box{background:#1d1f24;border:1px solid #444;border-radius:10px;padding:18px 20px;width:min(92vw,380px)}
  #fsa-net-lobby h2{margin:0 0 8px;font-size:18px} #fsa-net-lobby .code{font:600 28px/1.2 ui-monospace,monospace;letter-spacing:4px}
  #fsa-net-lobby ul{list-style:none;padding:0;margin:10px 0} #fsa-net-lobby li{padding:4px 0;border-bottom:1px solid #333}
  #fsa-net-lobby button{font:inherit;padding:10px 16px;border-radius:8px;border:0;background:#3b82f6;color:#fff;width:100%;margin-top:6px}
  #fsa-net-lobby .err{color:#f87171} #fsa-net-lobby .small{color:#aaa;font-size:13px;word-break:break-all}
  #fsa-net-chip{position:fixed;left:8px;top:8px;z-index:40;background:rgba(0,0,0,.55);color:#ddd;font:12px ui-monospace,monospace;padding:3px 7px;border-radius:6px;pointer-events:none}
  #fsa-net-banner{position:fixed;left:0;right:0;top:0;z-index:60;padding:10px 14px;font:600 15px system-ui,sans-serif;color:#fff;background:#b91c1c;text-align:center}
  #fsa-net-toast{position:fixed;left:50%;bottom:16%;transform:translateX(-50%);z-index:45;background:rgba(0,0,0,.7);color:#fff;padding:6px 12px;border-radius:8px;font:14px system-ui,sans-serif;display:none}`;
  document.head.appendChild(css);
  let lobby = null, chip = null, toastEl = null, toastTimer = 0;
  const box = () => {
    if (!lobby) {
      lobby = document.createElement('div');
      lobby.id = 'fsa-net-lobby';
      lobby.innerHTML = '<div class="box"></div>';
      document.body.appendChild(lobby);
    }
    return lobby.firstChild;
  };
  const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]);
  const api = {
    status(text) { box().innerHTML = `<h2>Online</h2><p>${esc(text)}</p>`; },
    error(text) { box().innerHTML = `<h2>Online</h2><p class="err">${esc(text)}</p><button onclick="location.reload()">Retry</button>`; },
    lobby(sess, onStart) {
      const link = new URL(location.href);
      link.searchParams.delete('seat');
      const seats = (sess.roster || []).map((s) => `<li>Player ${s.seat + 1}: ${s.here ? esc(s.name) + (s.seat === sess.seat ? ' (you)' : '') : '<span class="small">free</span>'}</li>`).join('');
      box().innerHTML = `<h2>Room</h2><div class="code">${esc(sess.room)}</div>
        <p class="small">Others open: ${esc(link.href)}</p>
        <ul>${seats}</ul>
        <p class="small">${esc(sess.opts.mode)}, ${sess.opts.seats} seats${sess.opts.script ? ', script ' + esc(sess.opts.script) : ''}</p>
        ${sess.host ? '<button id="fsa-net-start">Start</button>' : '<p>Waiting for player 1 to start...</p>'}`;
      const b = document.getElementById('fsa-net-start');
      if (b) b.onclick = () => { b.disabled = true; b.textContent = 'Starting...'; onStart(); };
    },
    hideLobby() { if (lobby) { lobby.remove(); lobby = null; } },
    chip(fn) {
      if (!chip) { chip = document.createElement('div'); chip.id = 'fsa-net-chip'; document.body.appendChild(chip); }
      setInterval(() => { chip.textContent = fn(); }, 500);
    },
    toast(text) {
      if (!toastEl) { toastEl = document.createElement('div'); toastEl.id = 'fsa-net-toast'; document.body.appendChild(toastEl); }
      toastEl.textContent = text; toastEl.style.display = 'block';
      clearTimeout(toastTimer); toastTimer = setTimeout(() => { toastEl.style.display = 'none'; }, 2500);
    },
    banner(text) {
      let b = document.getElementById('fsa-net-banner');
      if (!b) { b = document.createElement('div'); b.id = 'fsa-net-banner'; document.body.appendChild(b); }
      b.textContent = text;
    },
  };
  window.fsaNetUi = api;
  return api;
}
