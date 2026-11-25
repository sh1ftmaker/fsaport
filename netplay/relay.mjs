#!/usr/bin/env node
// The netplay relay (docs/NETPLAY.md): rooms with a code, seats, the start
// sequence, input fan-out, the hash referee, and timeline events (a seat that
// leaves). Every browser runs the whole game; this only moves inputs.
//
//   node netplay/relay.mjs [--port 8858] [--lat MS] [--jitter MS] [--quiet]
//
// No dependencies: a minimal WebSocket server (RFC 6455: text and binary
// frames, ping/close, no extensions) on node's http.
//
// Artificial latency for measurements: --lat/--jitter (every connection), or
// per connection ?lat=MS&jitter=MS on the WebSocket URL. The delay is applied
// to each direction separately and in FIFO order (a message never overtakes an
// earlier one, as on TCP; katam-port's NETPLAY-HANDOFF §6 harness pitfall).
//
// Protocol (version 1). Text frames are JSON {t: ...}; binary frames are inputs.
//   client -> relay
//     hello {v, room, create, seat, name, build, disc, opts}   join (or create) a room
//     start {}                                                 the host starts the game
//     ping {c}                                                 -> pong {c, s}
//     stat {head, rtt}                                         forwarded to the others with the seat
//     hash {n, h}                                              the state hash at read n
//   relay -> client
//     welcome {room, seat, host, opts, roster, started}
//     roster {seats: [{seat, name, here}], started}
//     start {opts, seats: [seat...]}
//     error {code, msg}            codes: build, disc, full, started, room, proto
//     stat {seat, head, rtt}
//     desync {n, hashes: {seat: h}}
//     leave {seat, at}             seat left: from frame `at` on its input is neutral
//   binary (both ways, little-endian):
//     0x01 u8 seat, u32 first, u16 count, u32 head, count x (u8 err, u32 a, u32 b)
//     (the inputs of `seat` for frames first..first+count-1; head = the sender's
//     read count when it sent them; the relay checks the seat and forwards it)
import http from 'node:http';
import crypto from 'node:crypto';

const args = {};
for (let i = 2; i < process.argv.length; i++) {
  const a = process.argv[i];
  if (a.startsWith('--')) args[a.slice(2)] = process.argv[i + 1] && !process.argv[i + 1].startsWith('--') ? process.argv[++i] : 'true';
}
const PORT = Number(args.port || 8858);
const LAT = Number(args.lat || 0), JITTER = Number(args.jitter || 0);
const log = args.quiet ? () => {} : (s) => console.log(new Date().toISOString().slice(11, 23) + ' ' + s);
const PROTO = 1;
const MSG_INPUT = 0x01;

// --- Minimal WebSocket server -------------------------------------------------
function accept(req, socket, onOpen) {
  const key = req.headers['sec-websocket-key'];
  if (!key || (req.headers.upgrade || '').toLowerCase() !== 'websocket') { socket.destroy(); return; }
  const acceptKey = crypto.createHash('sha1').update(key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest('base64');
  socket.write('HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n' +
               `Sec-WebSocket-Accept: ${acceptKey}\r\n\r\n`);
  socket.setNoDelay(true);
  const ws = { socket, open: true, onmessage: null, onclose: null };
  let buf = Buffer.alloc(0), frags = [], fragOp = 0;
  ws.send = (data) => {
    if (!ws.open) return;
    const bin = typeof data !== 'string';
    const payload = bin ? Buffer.from(data.buffer ? new Uint8Array(data.buffer, data.byteOffset, data.byteLength) : data) : Buffer.from(data);
    const n = payload.length;
    const head = n < 126 ? Buffer.from([bin ? 0x82 : 0x81, n])
      : n < 65536 ? Buffer.from([bin ? 0x82 : 0x81, 126, n >> 8, n & 255])
      : (() => { const h = Buffer.alloc(10); h[0] = bin ? 0x82 : 0x81; h[1] = 127; h.writeBigUInt64BE(BigInt(n), 2); return h; })();
    socket.write(Buffer.concat([head, payload]));
  };
  ws.close = () => {
    if (!ws.open) return;
    ws.open = false;
    try { socket.write(Buffer.from([0x88, 0])); socket.end(); } catch (e) { /* gone */ }
    if (ws.onclose) ws.onclose();
  };
  socket.on('data', (chunk) => {
    buf = Buffer.concat([buf, chunk]);
    for (;;) {
      if (buf.length < 2) return;
      const fin = buf[0] & 0x80, op = buf[0] & 15, masked = buf[1] & 0x80;
      let len = buf[1] & 127, off = 2;
      if (len === 126) { if (buf.length < 4) return; len = buf.readUInt16BE(2); off = 4; }
      else if (len === 127) { if (buf.length < 10) return; len = Number(buf.readBigUInt64BE(2)); off = 10; }
      const maskOff = off;
      if (masked) off += 4;
      if (buf.length < off + len) return;
      let payload = buf.subarray(off, off + len);
      if (masked) {
        const m = buf.subarray(maskOff, maskOff + 4);
        payload = Buffer.from(payload);
        for (let i = 0; i < payload.length; i++) payload[i] ^= m[i & 3];
      }
      buf = buf.subarray(off + len);
      if (op === 8) { ws.close(); return; }
      if (op === 9) { socket.write(Buffer.concat([Buffer.from([0x8A, payload.length]), payload])); continue; }
      if (op === 10) continue;
      if (op !== 0) { fragOp = op; frags = []; }
      frags.push(payload);
      if (!fin) continue;
      const data = Buffer.concat(frags);
      frags = [];
      if (ws.onmessage) ws.onmessage(fragOp === 1 ? data.toString('utf8') : data);
    }
  });
  socket.on('close', () => { if (ws.open) { ws.open = false; if (ws.onclose) ws.onclose(); } });
  socket.on('error', () => {});
  onOpen(ws);
}

// A connection with artificial latency: FIFO per direction.
function delayed(ws, lat, jitter) {
  if (!lat && !jitter) return { send: (d) => ws.send(d), recv: (fn) => fn };
  let lastOut = 0, lastIn = 0;
  const when = (last) => Math.max(last, Date.now() + lat + Math.random() * jitter);
  return {
    send: (d) => { const t = when(lastOut); lastOut = t; setTimeout(() => ws.send(d), t - Date.now()); },
    recv: (fn) => (d) => { const t = when(lastIn); lastIn = t; setTimeout(() => fn(d), t - Date.now()); },
  };
}

// --- Rooms ----------------------------------------------------------------------
const rooms = new Map();  // code -> room
const ALPHABET = 'BCDFGHJKLMNPQRSTVWXZ';
function newCode() {
  for (;;) {
    let c = '';
    for (let i = 0; i < 4; i++) c += ALPHABET[crypto.randomInt(ALPHABET.length)];
    if (!rooms.has(c)) return c;
  }
}

function makeRoom(code, hello) {
  const o = hello.opts || {};
  const seats = Math.max(1, Math.min(4, Number(o.seats) || 2));
  return {
    code, build: hello.build, disc: hello.disc,
    opts: { seats, mode: o.mode === 'lockstep' ? 'lockstep' : 'rollback', delay: o.delay ?? null,
            window: o.window ?? null, snapevery: o.snapevery ?? null, script: o.script || null,
            autostart: o.autostart ? Math.min(seats, Number(o.autostart)) : 0 },
    members: new Array(seats).fill(null),  // seat -> member
    started: false, startSeats: [],
    last: new Array(seats).fill(-1),       // highest frame relayed per seat
    left: new Map(),
    hashes: new Map(),                     // n -> Map(seat, h)
    desyncs: 0, inputs: 0, bytes: 0, created: Date.now(),
  };
}

function roster(r) {
  return { t: 'roster', started: r.started,
           seats: r.members.map((m, i) => ({ seat: i, name: m ? m.name : null, here: !!m })) };
}
function broadcast(r, msg, except) {
  const s = typeof msg === 'string' ? msg : JSON.stringify(msg);
  for (const m of r.members) if (m && m !== except) m.out.send(s);
}

function startRoom(r) {
  if (r.started) return;
  r.started = true;
  r.startSeats = r.members.map((m, i) => (m ? i : -1)).filter((i) => i >= 0);
  // Seats nobody took are neutral from frame 0 on, in every browser.
  r.members.forEach((m, i) => { if (!m) r.left.set(i, 0); });
  log(`room ${r.code}: start, seats ${r.startSeats.join(',')} of ${r.opts.seats}`);
  broadcast(r, { t: 'start', opts: r.opts, seats: r.startSeats, left: [...r.left].map(([seat, at]) => ({ seat, at })) });
}

function onHash(r, seat, n, h) {
  let m = r.hashes.get(n);
  if (!m) { m = new Map(); r.hashes.set(n, m); }
  m.set(seat, h);
  const live = r.startSeats.filter((s) => !r.left.has(s));
  if (live.every((s) => m.has(s))) {
    const vals = new Set(m.values());
    if (vals.size > 1) {
      r.desyncs++;
      const hashes = Object.fromEntries(m);
      log(`room ${r.code}: DESYNC at read ${n}: ${JSON.stringify(hashes)}`);
      broadcast(r, { t: 'desync', n, hashes });
    }
    r.hashes.delete(n);
  }
  if (r.hashes.size > 2000) r.hashes.delete(r.hashes.keys().next().value);
}

function onConnection(ws, url) {
  const lat = Number(url.searchParams.get('lat') || LAT), jitter = Number(url.searchParams.get('jitter') || JITTER);
  const io = delayed(ws, lat, jitter);
  const member = { out: io, name: '?', room: null, seat: -1 };
  const err = (code, msg) => { io.send(JSON.stringify({ t: 'error', code, msg })); setTimeout(() => ws.close(), 200 + lat); };
  ws.onmessage = io.recv((data) => {
    const r = member.room;
    if (typeof data !== 'string') {
      if (!r || data.length < 12 || data[0] !== MSG_INPUT || data[1] !== member.seat) return;
      const first = data.readUInt32LE(2), count = data.readUInt16LE(6);
      if (data.length !== 12 + 9 * count) return;
      r.last[member.seat] = Math.max(r.last[member.seat], first + count - 1);
      r.inputs += count; r.bytes += data.length;
      for (const m of r.members) if (m && m !== member) m.out.send(data);
      return;
    }
    let m;
    try { m = JSON.parse(data); } catch (e) { return; }
    if (m.t === 'hello') {
      if (m.v !== PROTO) return err('proto', `protocol ${m.v}, relay speaks ${PROTO}`);
      let code = String(m.room || '').toUpperCase();
      let room = rooms.get(code);
      if (!room) {
        if (!m.create) return err('room', `no room ${code}`);
        if (!code || code === 'NEW') code = newCode();
        room = makeRoom(code, m);
        rooms.set(code, room);
        log(`room ${code}: created (${room.opts.seats} seats, ${room.opts.mode}, build ${String(m.build).slice(0, 12)})`);
      }
      if (m.build !== room.build) return err('build', `this page is build ${String(m.build).slice(0, 12)}, the room's is ${String(room.build).slice(0, 12)}: reload both on the same build`);
      if (m.disc !== room.disc) return err('disc', `this disc (${m.disc}) is not the room's (${room.disc})`);
      if (room.started) return err('started', `room ${code} has started`);
      let seat = Number.isInteger(m.seat) && m.seat >= 0 ? m.seat : room.members.findIndex((x) => !x);
      if (seat < 0 || seat >= room.opts.seats) return err('full', `room ${code} is full`);
      if (room.members[seat]) return err('full', `seat ${seat + 1} of room ${code} is taken`);
      member.room = room; member.seat = seat; member.name = String(m.name || `player ${seat + 1}`).slice(0, 24);
      room.members[seat] = member;
      log(`room ${code}: seat ${seat} <- ${member.name} (lat ${lat} jitter ${jitter})`);
      io.send(JSON.stringify({ t: 'welcome', room: code, seat, host: seat === room.members.findIndex((x) => x),
                               opts: room.opts, started: room.started }));
      broadcast(room, roster(room));
      const here = room.members.filter((x) => x).length;
      if (room.opts.autostart && here >= room.opts.autostart) startRoom(room);
      return;
    }
    if (!r) return;
    if (m.t === 'start') { if (member.seat === r.members.findIndex((x) => x)) startRoom(r); return; }
    if (m.t === 'ping') { io.send(JSON.stringify({ t: 'pong', c: m.c, s: Date.now() })); return; }
    if (m.t === 'stat') { broadcast(r, { t: 'stat', seat: member.seat, head: m.head, rtt: m.rtt }, member); return; }
    if (m.t === 'hash') { if (r.started && Number.isInteger(m.n)) onHash(r, member.seat, m.n, m.h >>> 0); return; }
  });
  ws.onclose = () => {
    const r = member.room;
    if (!r || r.members[member.seat] !== member) return;
    r.members[member.seat] = null;
    if (r.started) {
      // A timeline event: after its last input this seat is neutral, the same
      // frame for everyone (inputs and this message leave the relay in order).
      const at = r.last[member.seat] + 1;
      r.left.set(member.seat, at);
      log(`room ${r.code}: seat ${member.seat} left at frame ${at}`);
      broadcast(r, { t: 'leave', seat: member.seat, at });
    } else {
      broadcast(r, roster(r));
    }
    if (r.members.every((x) => !x)) {
      log(`room ${r.code}: empty, closed (${r.inputs} inputs relayed, ${r.desyncs} desyncs)`);
      rooms.delete(r.code);
    }
  };
}

const server = http.createServer((req, res) => {
  // GET /rooms: a status page for debugging.
  const list = [...rooms.values()].map((r) => ({ code: r.code, started: r.started, seats: r.opts.seats, mode: r.opts.mode,
    here: r.members.filter((m) => m).length, inputs: r.inputs, desyncs: r.desyncs, last: r.last }));
  res.writeHead(200, { 'Content-Type': 'application/json', 'Access-Control-Allow-Origin': '*' });
  res.end(JSON.stringify({ relay: 'fsaport netplay', proto: PROTO, rooms: list }));
});
server.on('upgrade', (req, socket) => {
  const url = new URL(req.url, 'http://relay');
  accept(req, socket, (ws) => onConnection(ws, url));
});
server.listen(PORT, '127.0.0.1', () => log(`relay on ws://127.0.0.1:${PORT}/ (lat ${LAT} jitter ${JITTER})`));
