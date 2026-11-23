// --pre-js: the disc image, read in 1 MiB blocks with an LRU cache.
// Module.fsaDisc = fsaDiscFromFile(file) or await fsaDiscFromUrl(url).
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
