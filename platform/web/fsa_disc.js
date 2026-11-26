// --pre-js: the disc image, read in 1 MiB blocks with an LRU cache.
// Module.fsaDisc = fsaDiscFromFile(file) or await fsaDiscFromUrl(url); then
// await Module.fsaDolLoad(Module.fsaDisc) before the runtime starts (below).
const FSA_BLOCK = 1 << 20;
const FSA_CACHE_BLOCKS = 96;

function fsaMakeDisc(size, readBlock) {
  const cache = new Map();
  async function block(i) {
    let b = cache.get(i);
    if (b) { cache.delete(i); cache.set(i, b); return b; }
    b = readBlock(i * FSA_BLOCK, Math.min(FSA_BLOCK, size - i * FSA_BLOCK));
    cache.set(i, b);
    if (cache.size > FSA_CACHE_BLOCKS) cache.delete(cache.keys().next().value);
    return b;
  }
  return {
    size,
    async read(offset, length) {
      const out = new Uint8Array(length);
      let done = 0;
      while (done < length) {
        const pos = offset + done;
        const i = Math.floor(pos / FSA_BLOCK);
        const data = await block(i);
        const start = pos - i * FSA_BLOCK;
        const n = Math.min(length - done, data.length - start);
        if (n <= 0) break;
        out.set(data.subarray(start, start + n), done);
        done += n;
      }
      return out;
    },
  };
}

function fsaDiscFromFile(file) {
  return fsaMakeDisc(file.size, async (off, len) =>
    new Uint8Array(await file.slice(off, off + len).arrayBuffer()));
}

async function fsaDiscFromUrl(url) {
  const head = await fetch(url, { method: 'HEAD' });
  const size = Number(head.headers.get('Content-Length'));
  return fsaMakeDisc(size, async (off, len) => {
    const r = await fetch(url, { headers: { Range: `bytes=${off}-${off + len - 1}` } });
    return new Uint8Array(await r.arrayBuffer());
  });
}
Module.fsaDiscFromFile = fsaDiscFromFile;
Module.fsaDiscFromUrl = fsaDiscFromUrl;

// main.dol from the player's disc: the game's data with no source form is not
// in fsa.wasm (tools/gen_globals.py); platform/dol_data.cpp copies it from here
// in the first constructor. The runtime waits (run dependency 'fsa-dol') until
// Module.fsaDolLoad(disc) has read and checked main.dol.
const FSA_GAME_ID = 'G4SE01';
const FSA_DOL_SHA256 = 'de0bdc7727cbd50e427918bb7f72cdf81ed6e7ff1a2f8a8ff3c21a347d6b53d5';

function fsaSha256(bytes) {
  const K = new Uint32Array([
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2]);
  const H = new Uint32Array([0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19]);
  const n = bytes.length, total = Math.ceil((n + 9) / 64) * 64;
  const tail = new Uint8Array(total - (n & ~63));
  tail.set(bytes.subarray(n & ~63));
  tail[n & 63] = 0x80;
  const tv = new DataView(tail.buffer);
  tv.setUint32(tail.length - 8, Math.floor(n / 0x20000000));
  tv.setUint32(tail.length - 4, (n * 8) >>> 0);
  const W = new Uint32Array(64);
  const rotr = (x, r) => (x >>> r) | (x << (32 - r));
  function block(v, at) {
    for (let i = 0; i < 16; i++) W[i] = v.getUint32(at + i * 4);
    for (let i = 16; i < 64; i++) {
      const a = W[i - 15], b = W[i - 2];
      W[i] = (W[i - 16] + (rotr(a, 7) ^ rotr(a, 18) ^ (a >>> 3)) + W[i - 7] + (rotr(b, 17) ^ rotr(b, 19) ^ (b >>> 10))) | 0;
    }
    let [a, b, c, d, e, f, g, h] = H;
    for (let i = 0; i < 64; i++) {
      const t1 = (h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + W[i]) | 0;
      const t2 = ((rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c))) | 0;
      h = g; g = f; f = e; e = (d + t1) | 0; d = c; c = b; b = a; a = (t1 + t2) | 0;
    }
    H[0] += a; H[1] += b; H[2] += c; H[3] += d; H[4] += e; H[5] += f; H[6] += g; H[7] += h;
  }
  const bv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  for (let at = 0; at + 64 <= n; at += 64) block(bv, at);
  for (let at = 0; at < tail.length; at += 64) block(tv, at);
  return [...H].map((x) => x.toString(16).padStart(8, '0')).join('');
}

let fsaDolDep = false;
Module.preRun = [].concat(Module.preRun || []);
Module.preRun.push(() => {
  if (!Module.fsaDol) { addRunDependency('fsa-dol'); fsaDolDep = true; }
  if (Module.fsaCodeReady) Module.fsaCodeReady();
});

// Reads main.dol (boot.bin's DOL offset, 0x420) and checks it. Throws an Error
// with a message for the player when the disc is not the right one.
Module.fsaDolLoad = async (disc) => {
  if (Module.fsaDol) return;
  const be32 = (b, o) => ((b[o] << 24) | (b[o + 1] << 16) | (b[o + 2] << 8) | b[o + 3]) >>> 0;
  const boot = await disc.read(0, 0x440);
  const id = String.fromCharCode(...boot.subarray(0, 6));
  const magic = be32(boot, 0x1C);  // the GameCube disc magic
  if (magic !== 0xC2339F3D)
    throw new Error('This file is not a GameCube disc image. Choose your Four Swords Adventures disc image (.iso).');
  if (id !== FSA_GAME_ID)
    throw new Error(`This disc is ${id.replace(/[^\x20-\x7e]/g, '?')}, not Four Swords Adventures (USA, ${FSA_GAME_ID}). ` +
                    'Only the North American release is supported.');
  const off = be32(boot, 0x420);
  const head = await disc.read(off, 0x100);
  const secs = [];
  let size = 0x100;
  for (let i = 0; i < 18; i++) {
    const o = be32(head, i * 4), a = be32(head, 0x48 + i * 4), s = be32(head, 0x90 + i * 4);
    if (s) { secs.push({ o, a, s }); size = Math.max(size, o + s); }
  }
  if (off + size > disc.size) throw new Error('The disc image is truncated (main.dol runs past its end).');
  const bytes = (await disc.read(off, size)).slice();
  const hash = fsaSha256(bytes);
  if (hash !== FSA_DOL_SHA256)
    throw new Error(`This ${FSA_GAME_ID} disc's main.dol is not the one the port is built from ` +
                    `(SHA-256 ${hash.slice(0, 16)}…, expected ${FSA_DOL_SHA256.slice(0, 16)}…). ` +
                    'The image may be modified or damaged.');
  Module.fsaDol = { bytes, secs };
  if (Module.fsaLog) Module.fsaLog(`[disc] main.dol: ${size} bytes, SHA-256 ok`);
  if (fsaDolDep) { fsaDolDep = false; removeRunDependency('fsa-dol'); }
};

// platform/dol_data.cpp: DOL bytes at a virtual address -> wasm memory.
Module.fsaDolRead = (dst, addr, size) => {
  const d = Module.fsaDol;
  if (!d) return 0;
  for (const s of d.secs) {
    if (s.a <= addr && addr + size <= s.a + s.s) {
      HEAPU8.set(d.bytes.subarray(s.o + addr - s.a, s.o + addr - s.a + size), dst);
      return 1;
    }
  }
  return 0;
};
