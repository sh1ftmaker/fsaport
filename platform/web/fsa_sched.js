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
    return new Promise((resolve) => this.waiters.set(from, resolve));
  },
  resume(to, start) {
    if (start) {
      queueMicrotask(() => Module._fsa_thread_entry(to));
      return;
    }
    const resolve = this.waiters.get(to);
    if (!resolve) throw new Error('fsaSched: thread ' + to + ' is not suspended');
    this.waiters.delete(to);
    queueMicrotask(resolve);
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
