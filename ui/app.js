'use strict';

var h = picodom.h;

// clang-format off
function UI(rpc, state) {
  return h('div', {class: 'container'},
           webHeader(state),
           appWindow(rpc, state));
}

function webHeader() {
  return h('header', {class: 'webapp-shown'},
           h('p', null,
             h('h1', null, '#glitch'),
               'by naivesound'),
           h('ul', null,
             h('li', null, h('a', {href : 'http://naivesound.com/products/glitch'}, 'about')),
             h('li', null, h('a', {href : '#'}, 'examples')),
             h('li', null, h('a', {href : 'https://github.com/naivesound/glitch'}, 'github')),
             h('li', null, h('a', {href : 'https://github.com/naivesound/glitch/blob/master/API.md'}, 'help'))),
           h('div', {class: 'window-title-bar'},
             h('div', {class: 'window-title-bar-button'}),
             h('div', {class: 'window-title-bar-button'}),
             h('div', {class: 'window-title-bar-button'})));
}

function appWindow(rpc, state) {
  return h('div', {class: 'app-window'},
           editor(rpc, state),
           errorBar(rpc, state),
           toolbar(rpc, state));
}

function editor(rpc, state) {
  var oncreate = function(el) {
    rpc.editor = CodeMirror.fromTextArea(el, {
      lineNumbers : true,
      theme : 'material',
      autofocus : true,
      scrollbarStyle : 'simple',
    });
    rpc.editor.setValue(state.text);
    rpc.editor.on('change', function(cm, change) {
      rpc.changeText(cm.getValue());
    });
  };
  var el = h('div', {class: 'editor__wrapper-fixed'},
             h('div', {class: 'editor__wrapper-outer'},
               h('div', {class: 'editor__wrapper-inner'},
                 h('textarea', {class: 'editor__textarea', oncreate: oncreate}))));
  if (rpc.editor && state.text != rpc.editor.getValue()) {
    rpc.editor.setValue(state.text);
  }
  return el;
}

function errorBar(rpc, state) {
  return h('div', {class: 'error-dialog', style: {display: (state.error ? 'block' : 'none')}},
           h('div', {class: 'error-dialog-message'}, 'Syntax error!'));
}

function toolbar(rpc, state) {
  return h('div', {class: 'toolbar'},
           h('ul', null,
             h('li', null, h('div', {
               class: 'toolbar__btn desktop-only',
               onclick: rpc.newFile.bind(rpc),
             }, 'new')),
             h('li', null, h('div', {
               class: 'toolbar__btn desktop-only',
               onclick: rpc.loadFile.bind(rpc),
             }, 'load')),
             h('li', null, h('div', {
               class: 'toolbar__btn',
               onclick:rpc.togglePlayback.bind(rpc),
             }, (state.isPlaying ? 'pause' : 'play'))),
             h('li', null, h('div', {
               class: 'toolbar__btn',
               onclick: rpc.stop.bind(rpc),
             }, 'stop')),
             h('li', null, h('div', {class: 'toolbar__btn'}, 'help'))));
}
// clang-format on

function DesktopRPC() {
  document.body.classList.add('desktop-app');
  this.call = function(c) { window.external.invoke_(JSON.stringify(c)); };
  this.init = function() { this.call({cmd : 'init'}); };
  this.changeText = function(text) {
    this.call({cmd : 'changeText', text : text})
  };
  this.newFile = function() { this.call({cmd : 'newFile'}); };
  this.loadFile = function() { this.call({cmd : 'loadFile'}); };
  this.stop = function() { this.call({cmd : 'stop'}); };
  this.togglePlayback = function() { this.call({cmd : 'togglePlayback'}); };
  this.setVar = function(name, value) {
    this.call({cmd : 'setVar', name : name, value : value});
  };
  this.render = function(state) {
    return this.element = picodom.patch(
               this.oldNode, (this.oldNode = UI(this, state)), this.element);
  };
  return this;
}

function WebRPC() {
  var DEFAULT_SONG = "";
  // Download asm.js or .wasm core
  window.Module = window.Module || {};
  var script = document.createElement('script');
  script.type = 'text/javascript';
  script.async = false;
  if ('WebAssembly' in window) {
    script.src = 'glitch-loader.js';
  } else {
    script.src = 'glitch-asm.js';
  }
  document.getElementsByTagName('head')[0].appendChild(script);

  this.state = {
    text : '',
    isPlaying : false,
  };

  this.init = function() {
    var audioContext = new window.AudioContext();
    Module.ccall('glitch_init', null, [ 'number', 'number' ],
                 [ audioContext.sampleRate, 0 ]);
    var G = Module.ccall('glitch_create', null, [], []);
    this.g = G;
    if (navigator.requestMIDIAccess) {
      navigator.requestMIDIAccess().then(function(midi) {
        function enumerate() {
          var inputs = midi.inputs.values();
          for (var input = inputs.next(); input && !input.done;
               input = inputs.next()) {
            input.value.onmidimessage = function(m) {
              Module.ccall('glitch_midi', null,
                           [ 'number', 'number', 'number', 'number' ],
                           [ G, m.data[0], m.data[1], m.data[2] ]);
            };
          }
        };
        enumerate();
        midi.onstatechange = enumerate;
      });
    }

    var AUDIO_BUFFER_SIZE = 1024;
    var pcm = audioContext.createScriptProcessor(AUDIO_BUFFER_SIZE, 0, 1);
    var analyser = audioContext.createAnalyser();

    analyser.fftSize = 1024;
    analyser.smoothingTimeConstant = 0;
    analyser.connect(audioContext.destination);
    pcm.onaudioprocess = undefined;

    this.analyser = analyser;
    this.pcm = pcm;

    if (window.location.hash) {
      this.state.text = decodeURIComponent(window.location.hash.substring(1));
    } else if (localStorage.getItem('expr')) {
      this.state.text = localStorage.getItem('expr');
    } else {
      this.state.text = DEFAULT_SONG;
    }
    if (window.location.search === '?play') {
      this.togglePlayback();
    }
    this.changeText(this.state.text);
    this.render(this.state);
  };

  this.setVar = function(name, value) {
    Module.ccall &&
        Module.ccall('glitch_set', null, [ 'number', 'string', 'number' ],
                     [ this.g, name, value ]);
  };

  this.changeText = function(text) {
    var r = Module.ccall('glitch_compile', 'number',
                         [ 'number', 'string', 'number' ],
                         [ this.g, text, text.length ]);
    if (r != 0) {
      this.state.error = 'Syntax error';
    } else {
      this.state.error = null;
      window.location.hash = encodeURIComponent(text);
      localStorage.setItem('expr', text);
    }
    this.state.text = text;
    this.render(this.state);
  };

  this.newFile = function() {};
  this.loadFile = function() {};
  this.stop = function() {
    // TODO
  };

  this.togglePlayback = function() {
    this.state.isPlaying = !this.state.isPlaying;
    if (this.state.isPlaying) {
      this.pcm.connect(this.analyser);
      this.pcm.onaudioprocess = this.audioCallback.bind(this);
    } else {
      this.pcm.disconnect();
      this.pcm.onaudioprocess = undefined;
    }
    this.render(this.state);
  };

  this.audioCallback = function(e) {
    var len = e.outputBuffer.length;
    var n = len * Float32Array.BYTES_PER_ELEMENT;
    var p = Module._malloc(n);
    var heap = new Uint8Array(Module.HEAPU8.buffer, p, n);
    heap.set(new Uint8Array(buffer.buffer));

    Module.ccall('glitch_fill', null,
                 [ 'number', 'number', 'number', 'number' ],
                 [ this.g, heap.byteOffset, len, 1 ]);
    var result = new Float32Array(heap.buffer, heap.byteOffset, len);
    e.outputBuffer.copyToChannel(result, 0, 0);
    Module._free(p);
  };

  this.render = function(state) {
    return this.element = picodom.patch(
               this.oldNode, (this.oldNode = UI(this, state)), this.element);
  };
  Module['onRuntimeInitialized'] = this.init.bind(this);
  return this;
}

var rpc;
window.onload = function() {
  if (window.external) {
    rpc = new DesktopRPC();
    rpc.init();
  } else {
    rpc = new WebRPC();
  }

  // Ctrl+Enter to toggle playback
  document.onkeydown = function(e) {
    if (e.keyCode === 13 && e.ctrlKey) {
      e.preventDefault();
      rpc.togglePlayback();
    }
  };

  // Ctrl+mouse move to change X/Y values
  document.onmousemove = function(e) {
    if (e.ctrlKey) {
      var x = e.pageX / window.innerWidth;
      var y = 1 - (e.pageY / window.innerHeight);
      rpc.setVar('x', x);
      rpc.setVar('y', y);
    }
  };
}
