// The chosen disc image, kept in this browser's Origin Private File System
// (OPFS), so that a reload (or a later visit) starts without choosing it again.
// The image stays on this device: OPFS is private to the page's origin and is
// never uploaded. window.fsaDiscStore:
//   load()        -> the stored File, or null (none, incomplete, or no OPFS)
//   save(file, progress(done, total)) -> stores a copy; rejects with an Error
//                    whose message is for the player (no space, not supported)
//   forget()      -> deletes the copy
//   info()        -> {name, size} of the stored image, or null
// Writing uses FileSystemFileHandle.createWritable() where the browser has it
// (Chrome, Edge, Firefox, Safari 26+), else a worker with a sync access handle
// (older Safari, including iPhone). The metadata file (disc.json) is written
// only once the copy is complete, so a reload during the copy re-picks.
(function () {
  'use strict';
  const DATA = 'disc.iso', META = 'disc.json';
  const ok = !!(navigator.storage && navigator.storage.getDirectory);
  const root = () => navigator.storage.getDirectory();

  async function readMeta() {
    try {
      const f = await (await (await root()).getFileHandle(META)).getFile();
      return JSON.parse(await f.text());
    } catch (e) { return null; }
  }

  async function remove(name) {
    try { await (await root()).removeEntry(name); } catch (e) { /* not there */ }
  }

  async function writeText(name, text) {
    const fh = await (await root()).getFileHandle(name, { create: true });
    if (fh.createWritable) {
      const w = await fh.createWritable();
      await w.write(text);
      await w.close();
      return;
    }
    await viaWorker({ name, text });
  }

  // Older Safari: no createWritable on the main thread; a dedicated worker can
  // write through createSyncAccessHandle().
  const WORKER_SRC = `
    onmessage = async (e) => {
      const { name, file, text } = e.data;
      let h = null;
      try {
        const dir = await navigator.storage.getDirectory();
        const fh = await dir.getFileHandle(name, { create: true });
        h = await fh.createSyncAccessHandle();
        h.truncate(0);
        if (text !== undefined) {
          h.write(new TextEncoder().encode(text), { at: 0 });
        } else {
          const CH = 8 << 20;
          let last = 0;
          for (let off = 0; off < file.size; ) {
            const b = new Uint8Array(await file.slice(off, off + CH).arrayBuffer());
            const n = h.write(b, { at: off });
            if (n !== b.length) throw new DOMException('short write', 'QuotaExceededError');
            off += n;
            if (off - last >= (64 << 20) || off === file.size) { last = off; postMessage({ progress: off }); }
          }
        }
        h.flush();
        h.close();
        postMessage({ done: true });
      } catch (err) {
        try { if (h) h.close(); } catch (e2) {}
        postMessage({ error: err && err.name, message: String(err && err.message || err) });
      }
    };`;
  function viaWorker(msg, progress) {
    return new Promise((resolve, reject) => {
      let w;
      try { w = new Worker(URL.createObjectURL(new Blob([WORKER_SRC], { type: 'text/javascript' }))); }
      catch (e) { reject(e); return; }
      w.onmessage = (e) => {
        const d = e.data;
        if (d.progress !== undefined) { if (progress) progress(d.progress); return; }
        w.terminate();
        if (d.done) resolve();
        else { const err = new Error(d.message); err.name = d.error || 'Error'; reject(err); }
      };
      w.onerror = (e) => { w.terminate(); reject(new Error(e.message || 'worker failed')); };
      w.postMessage(msg);
    });
  }

  const quotaError = (e) => e && (e.name === 'QuotaExceededError' || /quota|space|full/i.test(e.message || ''));

  window.fsaDiscStore = {
    supported: ok,
    async info() { return ok ? readMeta() : null; },
    async load() {
      if (!ok) return null;
      const meta = await readMeta();
      if (!meta || !meta.complete) return null;
      try {
        const f = await (await (await root()).getFileHandle(DATA)).getFile();
        if (f.size !== meta.size) return null;
        // A File named like the original, for the log and the status line.
        return new File([f], meta.name || 'disc.iso', { lastModified: meta.lastModified || 0 });
      } catch (e) { return null; }
    },
    async forget() {
      if (!ok) return;
      await remove(META);
      await remove(DATA);
    },
    async save(file, progress) {
      if (!ok) throw new Error('This browser cannot keep the disc image (no Origin Private File System); you will choose it again next time.');
      await remove(META);
      if (navigator.storage.estimate) {
        try {
          const { usage = 0, quota = 0 } = await navigator.storage.estimate();
          const old = await (async () => { try { return (await (await (await root()).getFileHandle(DATA)).getFile()).size; } catch (e) { return 0; } })();
          if (quota && quota - usage + old < file.size + (16 << 20))
            throw Object.assign(new Error('quota'), { name: 'QuotaExceededError' });
        } catch (e) {
          if (quotaError(e)) {
            await remove(DATA);
            throw new Error(`Not enough browser storage to keep the disc image (${Math.round(file.size / 1048576)} MB); you will choose it again next time.`);
          }
        }
      }
      if (navigator.storage.persist) navigator.storage.persist().catch(() => {});
      try {
        const fh = await (await root()).getFileHandle(DATA, { create: true });
        if (fh.createWritable) {
          const w = await fh.createWritable();
          try {
            const CH = 8 << 20;
            let last = 0;
            for (let off = 0; off < file.size; off += CH) {
              await w.write(new Uint8Array(await file.slice(off, off + CH).arrayBuffer()));
              const done = Math.min(file.size, off + CH);
              if (progress && (done - last >= (64 << 20) || done === file.size)) { last = done; progress(done, file.size); }
            }
            await w.close();
          } catch (e) {
            try { await w.abort(); } catch (e2) {}
            throw e;
          }
        } else {
          await viaWorker({ name: DATA, file }, (done) => progress && progress(done, file.size));
        }
        await writeText(META, JSON.stringify({ name: file.name, size: file.size, lastModified: file.lastModified, complete: true }));
      } catch (e) {
        await remove(META);
        await remove(DATA);
        if (quotaError(e))
          throw new Error(`Not enough browser storage to keep the disc image (${Math.round(file.size / 1048576)} MB); you will choose it again next time.`);
        throw new Error(`The disc image could not be kept in this browser (${e.name || 'Error'}: ${e.message}); you will choose it again next time.`);
      }
    },
  };
})();
