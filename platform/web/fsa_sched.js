// --pre-js: the page side of the platform layer.
//
// fsaSched: the coroutine switch. Each OS thread is a wasm stack suspended on a
// promise; switching resolves the target's promise (or starts it through the
// fsa_thread_entry promising export) from a microtask, after the caller has
// suspended.
//
// fsaClock: VI retraces at 59.94 Hz from requestAnimationFrame, whatever the
// display's refresh rate, plus setTimeout for alarms between retraces.
Module.fsaSched = {
  waiters: new Map(),
  switchTo(from, to, start) {
    return new Promise((resolve) => {
      this.waiters.set(from, resolve);
      queueMicrotask(() => this.resume(to, start));
    });
  },
  idle(from) {
    queueMicrotask(() => Module.fsaClock.armTimer());
    return new Promise((resolve) => this.waiters.set(from, resolve));
  },
  // Switches run as microtasks, which the browser drains before any task, so
  // threads that keep handing over to each other would starve rendering, input
  // and timers. Every ~8 ms a switch goes through a MessageChannel task instead.
  lastTask: 0,
  channel: null,
  later: [],
  defer(fn) {
    const now = performance.now();
    if (now - this.lastTask < 8) { queueMicrotask(fn); return; }
    if (!this.channel) {
      this.channel = new MessageChannel();
      this.channel.port1.onmessage = () => {
        this.lastTask = performance.now();
        const fns = this.later; this.later = [];
        for (const f of fns) f();
      };
    }
    this.later.push(fn);
    if (this.later.length === 1) this.channel.port2.postMessage(0);
  },
  resume(to, start) {
    if (start) {
      this.defer(() => Module._fsa_thread_entry(to));
      return;
    }
    const resolve = this.waiters.get(to);
    if (!resolve) throw new Error('fsaSched: thread ' + to + ' is not suspended');
    this.waiters.delete(to);
    this.defer(resolve);
  },
};

Module.fsaClock = {
  period: 1000 / 59.94,
  next: 0,
  timer: 0,
  start() {
    this.next = performance.now() + this.period;
    const tick = (now) => {
      if (now >= this.next) {
        // At most one retrace per animation frame; drop time we cannot catch up.
        this.next += this.period;
        if (now - this.next > this.period * 2) this.next = now + this.period;
        Module._fsa_retrace();
        this.armTimer();
      }
      requestAnimationFrame(tick);
    };
    requestAnimationFrame(tick);
  },
  armTimer() {
    const ms = Module._fsa_next_timer_ms();
    if (ms < 0 || ms > this.period) return;
    clearTimeout(this.timer);
    this.timer = setTimeout(() => { Module._fsa_timer(); this.armTimer(); }, ms);
  },
};
