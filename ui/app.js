'use strict';

var h = picodom.h;

function isWebView() {
  try {
    window.external.invoke('');
    return true;
  } catch (e) {
    return false;
  }
}

// clang-format off
function UI(app) {
  return h('div', {class: 'container'},
           webHeader(app),
           appWindow(app),
           webFooter(app));
}

function webHeader() {
  return h('header', {class: 'webapp-shown'},
           h('p', null,
             h('h1', null, '#glitch'),
               'by naivesound'),
           h('ul', null,
             h('li', null, h('a', {target: '_blank', href: 'http://naivesound.com/products/glitch'}, 'about')),
             h('li', null, h('a', {target: '_blank', href: 'https://soundcloud.com/naivesound'}, 'examples')),
             h('li', null, h('a', {target: '_blank', href: 'https://github.com/naivesound/glitch'}, 'github')),
             h('li', null, h('a', {target: '_blank', href: 'https://github.com/naivesound/glitch/blob/master/API.md'}, 'help'))),
           h('div', {class: 'window-title-bar'},
             h('div', {class: 'window-title-bar-button'}),
             h('div', {class: 'window-title-bar-button'}),
             h('div', {class: 'window-title-bar-button'})));
}

function webFooter() {
  return h('footer');
}

function appWindow(app) {
  return h('div', {class: 'app-window'},
           editor(app),
           modal(app.modalName == 'settings', app, settings(app)),
           errorBar(app),
           toolbar(app));
}

function editor(app) {
  var tid;
  var oncreate = function(el) {
    CodeMirror.defineMode("glitch", function() {
      function tokenize(stream, state) {
        var c = stream.next();
        if (/\d/.test(c)) {
          stream.eatWhile(/[\d\.]/);
          return "number";
        } else if (c == "#") {
          stream.skipToEnd();
          return "comment";
        } else if (/[-+*\/%&|^!=<>,]/.test(c)) {
          return "operator";
        } else if (c == '(' || c == ')') {
          return "operator"
        } else if (c == '$') {
          if (stream.eat(/\d+/)) {
            return "variable-3";
          }
          return "builtin"
        } else {
          stream.eatWhile(/[\w\d_#]+/)
          stream.eatSpace();
          if (stream.peek() == '(') {
            var s = stream.current();
            if (/hz|sin|tri|saw|sqr|lpf|hpf|bpf|bsf|tr808|pluck|fm|delay|end|mix|seq|loop|a|s|l|r/.test(s)) {
              return "keyword";
            }
            return "variable-2";
          }
          return "variable";
        }
      }
      return {
        startState: function() {
          return {
            tokenize: tokenize,
          };
        },
        token: function(stream, state) {
          if (stream.eatSpace()) return null;
          return state.tokenize(stream, state);
        },
      };
    });
    app.editor = CodeMirror.fromTextArea(el, {
      lineNumbers : true,
      theme : 'material',
      autofocus : true,
      matchBrackets: true,
      autoCloseBrackets: true,
      scrollbarStyle : 'simple',
    });
    app.editor.setValue(app.data.text);
    app.editor.on('change', function(cm, change) {
      clearTimeout(tid);
      tid = setTimeout(function() {
        app.changeText(cm.getValue());
      }, 250);
    });
  };
  var el = h('div', {class: 'editor__wrapper-fixed'},
             h('div', {class: 'editor__wrapper-outer'},
               h('div', {class: 'editor__wrapper-inner'},
                 h('textarea', {class: 'editor__textarea', oncreate: oncreate}))));
  if (app.editor && app.data.text != app.editor.getValue()) {
    app.editor.setValue(app.data.text);
  }
  return el;
}

function errorBar(app) {
  return h('div', {class: 'error-dialog', style: {display: (app.data.error ? 'block' : 'none')}},
           materialIcon('\uE000'));
}

function modal(visible, app, contents) {
  return h('div', {
            class: 'modal-background',
            style: {display: (visible ? 'block' : 'none')},
            onclick: function() { app.modal(); },
          }, h('div', {
            class: 'modal',
            onclick: function(e) {
              e.preventDefault();
              e.stopPropagation();
            }
          }, contents));
}

function settings(app) {
  var sampleRates = [];
  if (app.data.audioDevices[app.data.audioDevice]) {
    sampleRates = app.data.audioDevices[app.data.audioDevice].sampleRates;
  }
  return h('div', null,
           h('div', {class: 'modal-header'}, 'SETTINGS',
             h('div', {class: 'modal-header-close', onclick: function() { app.modal(); }}, '×')),
           h('div', {class: 'modal-content'},
             h('form', null,
               h('div', {class: 'modal-form-group'},
                 h('label', {class: 'modal-form-label', for: 'audio-device-select'}, 'Audio device:'),
                 h('select', {
                   id: 'audio-device-select',
                   class: 'modal-form-value',
                   value: ''+app.data.audioDevice,
                   onchange: function() {
                     app.selectAudio(+this.value, app.data.sampleRate, app.data.bufferSize);
                   }
                 }, app.data.audioDevices.map(function(el) {
                   return h('option', {value: ''+el.id}, el.name);
                 }))),
               h('div', {class: 'modal-form-group'},
                 h('label', {class: 'modal-form-label', for: 'sample-rate-select'}, 'Sample rate:'),
                 h('select', {
                   id: 'sample-rate-select',
                   class: 'modal-form-value',
                   value: app.data.sampleRate,
                   onchange: function() {
                     app.selectAudio(app.data.audioDevice, +this.value, app.data.bufferSize);
                   }
                 }, sampleRates.map(function(el) {
                   return h('option', {value: el}, el);
                 }))),
               h('div', {class: 'modal-form-group'},
                 h('label', {class: 'modal-form-label', for: 'buffer-size-select'}, 'Buffer size:'),
                 h('select', {
                   id: 'buffer-size-select',
                   class: 'modal-form-value',
                   value: app.data.bufferSize,
                   onchange: function() {
                     app.selectAudio(app.data.audioDevice, app.data.sampleRate, +this.value);
                   }
                 }, [64, 128, 256, 512, 1024, 2048, 4096, 8192].map(function(el) {
                   return h('option', {value: el}, el + '');
                 })))),
             h('div', null,
               h('div', {class: 'modal-form-group'},
                 h('label', {class: 'modal-form-label'}, 'MIDI devices:'),
                 h('div', {class: 'modal-form-value'},
                   app.data.midiDevices.length == 0 ?
                     h('div', null, '(none)') :
                     app.data.midiDevices.map(function(el, i) {
                       var id = 'midi-'+i;
                       return h('div', null,
                                h('label', {'for': id},
                                  h('input', {
                                    type: 'checkbox',
                                    checked: el.connected,
                                    name: id,
                                    id: id,
                                    onchange: function() {
                                      app.selectMIDI(el.name, !el.connected);
                                    },
                                  }),
                                  el.name));
                   }))))));
}

function toolbar(app) {
  var icons = {
    add: materialIcon('\uE145'),
    folderOpen: materialIcon('\uE2C8'),
    playArrow: materialIcon('\uE037'),
    pause: materialIcon('\uE034'),
    stop: materialIcon('\uE047'),
    settings: materialIcon('\uE8B8'),
    help: materialIcon('\uE8FD'),
  };
  return h('div', {class: 'toolbar'},
           h('ul', null,
             h('li', null, h('div', {
               class: 'toolbar__btn desktop-only',
               onclick: app.newFile.bind(app),
             }, icons.add)),
             h('li', null, h('div', {
               class: 'toolbar__btn desktop-only',
               onclick: app.loadFile.bind(app),
             }, icons.folderOpen)),
             h('li', null, h('div', {
               class: 'toolbar__btn',
               onclick:app.togglePlayback.bind(app),
             }, (app.data.isPlaying ? icons.pause : icons.playArrow))),
             h('li', null, h('div', {
               class: 'toolbar__btn',
               onclick: app.stop.bind(app),
             }, icons.stop)),
             h('li', null, h('div', {
               class: 'toolbar__btn desktop-only',
               onclick: function() { app.modal('settings'); },
             }, icons.settings)),
             h('li', null, h('div', {
               class: 'toolbar__btn',
               onclick: function() { window.open('https://github.com/naivesound/glitch/blob/master/API.md'); },
             }, icons.help))));
}

function materialIcon(icon) {
  return h('i', {class: 'material-icons', style:{display: 'inline', fontSize: '32px', lineHeight: '64px'}}, icon);
}

function hideLoadingIndicator() {
  document.getElementById('loading-spinner').style.display = 'none';
}
// clang-format on

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

  var audioContext = new window.AudioContext();
  this.data = {
    text: '',
    isPlaying: false,
    audioDevices: [],
    midiDevices: [],
    sampleRate: audioContext.sampleRate,
    audioDevice: '',
  };

  this.init = function() {
    Module.ccall(
        'glitch_init', null, ['number', 'number'],
        [audioContext.sampleRate, 0]);
    var G = Module.ccall('glitch_create', null, [], []);
    this.g = G;
    if (navigator.requestMIDIAccess) {
      navigator.requestMIDIAccess().then(function(midi) {
        function enumerate() {
          var inputs = midi.inputs.values();
          for (var input = inputs.next(); input && !input.done;
               input = inputs.next()) {
            input.value.onmidimessage = function(m) {
              Module.ccall(
                  'glitch_midi', null, ['number', 'number', 'number', 'number'],
                  [G, m.data[0], m.data[1], m.data[2]]);
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
      this.data.text = decodeURIComponent(window.location.hash.substring(1));
    } else if (localStorage.getItem('expr')) {
      this.data.text = localStorage.getItem('expr');
    } else {
      this.data.text = DEFAULT_SONG;
    }
    if (window.location.search === '?play') {
      this.togglePlayback();
    }
    this.changeText(this.data.text);
    hideLoadingIndicator();
    this.render();
  };

  this.refresh = function() {};

  this.setVar = function(name, value) {
    Module.ccall &&
        Module.ccall(
            'glitch_set', null, ['number', 'string', 'number'],
            [this.g, name, value]);
  };

  this.changeText = function(text) {
    var r = Module.ccall(
        'glitch_compile', 'number', ['number', 'string', 'number'],
        [this.g, text, text.length]);
    if (r != 0) {
      this.data.error = 'Syntax error';
    } else {
      this.data.error = null;
      window.location.hash = encodeURIComponent(text);
      localStorage.setItem('expr', text);
    }
    this.data.text = text;
    this.render();
  };

  this.newFile = function() {};
  this.loadFile = function() {};
  this.stop = function() {
    Module.ccall('glitch_reset', null, ['number'], [this.g]);
    if (this.data.isPlaying) {
      this.togglePlayback();
    }
    this.changeText(this.data.text);
    this.render();
  };

  this.togglePlayback = function() {
    this.data.isPlaying = !this.data.isPlaying;
    if (this.data.isPlaying) {
      this.pcm.connect(this.analyser);
      this.pcm.onaudioprocess = this.audioCallback.bind(this);
    } else {
      this.pcm.disconnect();
      this.pcm.onaudioprocess = undefined;
    }
    this.render();
  };

  this.audioCallback = function(e) {
    var len = e.outputBuffer.length;
    var n = len * Float32Array.BYTES_PER_ELEMENT;
    var p = Module._malloc(n);
    var heap = new Uint8Array(Module.HEAPU8.buffer, p, n);
    heap.set(new Uint8Array(buffer.buffer));

    Module.ccall(
        'glitch_fill', null, ['number', 'number', 'number', 'number'],
        [this.g, heap.byteOffset, len, 1]);
    var result = new Float32Array(heap.buffer, heap.byteOffset, len);
    e.outputBuffer.copyToChannel(result, 0, 0);
    Module._free(p);
  };

  Module['onRuntimeInitialized'] = this.init.bind(this);
  return this;
}

var app;

function init() {
  app.render = function() {
    return app.element = picodom.patch(
               app.oldNode, (app.oldNode = UI(app, app.data)), app.element);
  };

  app.modal = function(name) {
    app.modalName = name;
    app.render();
  };

  // Ctrl+Enter to toggle playback
  document.onkeydown = function(e) {
    if (e.keyCode === 27 &&
        app.modalName) {  // Esc - close current modal, if any
      e.preventDefault();
      app.modal();
    } else if (e.keyCode === 78 && e.ctrlKey) {  // Ctrl+N
      e.preventDefault();
      app.newFile();
    } else if (e.keyCode === 79 && e.ctrlKey) {  // Ctrl+O
      e.preventDefault();
      app.loadFile();
    } else if (e.keyCode === 13 && e.ctrlKey) {  // Ctrl+Enter
      e.preventDefault();
      app.togglePlayback();
    }
  };

  // Ctrl+mouse move to change X/Y values
  document.onmousemove = function(e) {
    if (e.ctrlKey) {
      var x = e.pageX / window.innerWidth;
      var y = 1 - (e.pageY / window.innerHeight);
      app.setVar('x', x);
      app.setVar('y', y);
    }
  };
}

if (isWebView()) {
  document.body.classList.add('desktop-app');
  hideLoadingIndicator();
  window.external.invoke('__app_js_loaded__');
} else {
  app = new WebRPC();
  init();
}
