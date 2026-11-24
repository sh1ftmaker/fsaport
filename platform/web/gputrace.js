// WebGPU call tracer for browsers that only say "encoder state is not valid"
// (WebKit): with ?gputrace=1 (or =N frames' worth of calls), every call on
// the device, queue, command and pass encoders is logged with a summary of
// its arguments, until the budget runs out. Loaded before the game.
(function () {
  'use strict';
  var q = /[?&]gputrace=(\d+)/.exec(location.search);
  if (!q || !window.GPUDevice) return;
  var budget = Math.max(200, Number(q[1]) > 1 ? Number(q[1]) : 1500);
  var ids = new WeakMap(), next = 1;
  function id(o, kind) {
    var n = ids.get(o);
    if (!n) { n = kind + (next++); ids.set(o, n); }
    return n;
  }
  function sum(v, depth) {
    if (v === null || v === undefined) return String(v);
    var t = typeof v;
    if (t === 'number') return Number.isInteger(v) ? String(v) : v.toFixed(3);
    if (t === 'string') return JSON.stringify(v);
    if (t === 'boolean') return String(v);
    if (window.GPUTexture && v instanceof GPUTexture)
      return id(v, 'tex') + '(' + v.width + 'x' + v.height + 'x' + v.depthOrArrayLayers + ' ' + v.format +
        ' mips ' + v.mipLevelCount + ' samples ' + v.sampleCount + ' usage ' + v.usage + ')';
    if (window.GPUBuffer && v instanceof GPUBuffer) return id(v, 'buf') + '(' + v.size + ' usage ' + v.usage + ')';
    if (window.GPUTextureView && v instanceof GPUTextureView) return id(v, 'view');
    if (window.GPURenderPipeline && v instanceof GPURenderPipeline) return id(v, 'pipe');
    if (window.GPUBindGroup && v instanceof GPUBindGroup) return id(v, 'bg');
    if (window.GPUSampler && v instanceof GPUSampler) return id(v, 'smp');
    if (ArrayBuffer.isView(v)) return v.constructor.name + '[' + v.length + ']';
    if (v instanceof ArrayBuffer) return 'ArrayBuffer[' + v.byteLength + ']';
    if (depth > 3) return '…';
    if (Array.isArray(v)) return '[' + v.map(function (x) { return sum(x, depth + 1); }).join(', ') + ']';
    if (t === 'object') {
      var out = [];
      for (var k in v) {
        if (k === 'code' && typeof v[k] === 'string') { out.push('code:' + v[k].length + 'ch'); continue; }
        out.push(k + ':' + sum(v[k], depth + 1));
      }
      return '{' + out.join(' ') + '}';
    }
    return t;
  }
  function wrap(proto, cls, kind) {
    if (!proto) return;
    Object.getOwnPropertyNames(proto).forEach(function (name) {
      var d = Object.getOwnPropertyDescriptor(proto, name);
      if (!d || typeof d.value !== 'function' || name === 'constructor') return;
      var orig = d.value;
      proto[name] = function () {
        var r = orig.apply(this, arguments);
        if (budget > 0) {
          budget--;
          var args = [];
          for (var i = 0; i < arguments.length; i++) args.push(sum(arguments[i], 0));
          var res = r && typeof r === 'object' && !(r instanceof Promise) ? ' -> ' + sum(r, 3) : '';
          if (r && /Encoder$/.test(r.constructor && r.constructor.name)) res = ' -> ' + id(r, 'enc');
          var line = '[gpu] ' + id(this, kind) + '.' + name + '(' + args.join(', ') + ')' + res;
          (window.fsaLog || console.log)(line.length > 900 ? line.slice(0, 900) + '…' : line);
          if (budget === 0) (window.fsaLog || console.log)('[gpu] trace budget used up');
        }
        return r;
      };
    });
  }
  wrap(GPUDevice.prototype, 'GPUDevice', 'dev');
  wrap(window.GPUQueue && GPUQueue.prototype, 'GPUQueue', 'queue');
  wrap(window.GPUCommandEncoder && GPUCommandEncoder.prototype, 'GPUCommandEncoder', 'enc');
  wrap(window.GPURenderPassEncoder && GPURenderPassEncoder.prototype, 'GPURenderPassEncoder', 'pass');
  wrap(window.GPUComputePassEncoder && GPUComputePassEncoder.prototype, 'GPUComputePassEncoder', 'cpass');
  wrap(window.GPUCanvasContext && GPUCanvasContext.prototype, 'GPUCanvasContext', 'ctx');
})();
