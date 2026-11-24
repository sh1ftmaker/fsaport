// Memory card persistence: Aurora keeps the cards under /save/GC/ (see
// extern/aurora/lib/card/DolphinCardPath.cpp). /save is mounted as IDBFS,
// filled from IndexedDB before main() and written back every few seconds and
// when the page is hidden, so saves survive a reload. ?nosave=1 starts with
// an empty card that is not written back.
Module.fsaSaveInit = function () {
  var log = function (s) { (window.fsaLog || console.log)('[save] ' + s); };
  var off = /[?&]nosave=1/.test(location.search);
  try { FS.mkdir('/save'); } catch (e) { /* exists */ }
  if (off || typeof IDBFS === 'undefined') {
    try { FS.mkdir('/save/GC'); } catch (e) { /* exists */ }
    log(off ? 'nosave: card kept in memory only' : 'no IDBFS: card kept in memory only');
    return Promise.resolve();
  }
  FS.mount(IDBFS, {}, '/save');
  var busy = false, again = false;
  function sync(why) {
    if (busy) { again = true; return; }
    busy = true;
    FS.syncfs(false, function (err) {
      busy = false;
      if (err) log('write failed (' + why + '): ' + err);
      if (again) { again = false; sync('queued'); }
    });
  }
  Module.fsaSaveSync = sync;
  return new Promise(function (resolve) {
    FS.syncfs(true, function (err) {
      if (err) log('load failed: ' + err);
      try { FS.mkdir('/save/GC'); } catch (e) { /* exists */ }
      var files = [];
      (function walk(dir) {
        FS.readdir(dir).forEach(function (n) {
          if (n === '.' || n === '..') return;
          var p = dir + '/' + n, st = FS.stat(p);
          if (FS.isDir(st.mode)) walk(p); else files.push(p + ' (' + st.size + ' bytes)');
        });
      })('/save');
      log(files.length ? 'loaded ' + files.join(', ') : 'no saved card yet');
      setInterval(function () { sync('timer'); }, 3000);
      addEventListener('pagehide', function () { sync('pagehide'); });
      document.addEventListener('visibilitychange', function () {
        if (document.visibilityState === 'hidden') sync('hidden');
      });
      resolve();
    });
  });
};
