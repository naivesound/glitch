"use strict";

if (typeof module !== "undefined") {
  var m = require('./mithril.js');
}

//
// Color constants
//
var YELLOW = '#ffc107';
var GRAY = '#333333';
var WHITE = '#ffffff';
var PINK = '#e91e63';
var GREEN = '#cddc39';

//
// Layout component
//
var Layout = {
  oninit: function(c) {
    c.onresize = () => {
      c.state.fullscreen = (window.innerWidth < 800 || window.innerHeight < 500 ||
                         (window.process && window.process.versions['electron']));
      m.redraw();
    };
    c.onunload = () => {
      window.removeEventListener('resize', c.onresize);
    };
    c.onresize();
    window.addEventListener('resize', c.onresize);
  },
  view: (c) => {
    let app = m(App, {tab: c.attrs.tab, glitch: c.attrs.glitch});
    if (!c.state.fullscreen) {
      app = m('.layout-flex-column',
              m(Links),
              m('.app-container', app),
              m(Copyright));
    }
    return m('.layout', app);
  }
};

var Links = {
  view: () =>
    m('.social-links',
      m('a[href=http://naivesound.com/]', 'about'),
      m.trust('&nbsp;~&nbsp'),
      m('a[href=http://twitter.com/naive_sound]', 'follow'),
      m.trust('&nbsp;~&nbsp'),
      m('a[href=https://github.com/naivesound/glitch]', 'github'))
};

var Copyright = {
  view: () =>
    m('.copyright',
      'Made with ',
      m('i.material-icons', {style:{color: PINK, fontSize: 'inherit'}}, 'favorite'),
      ' at ',
      m('a[href=http://naivesound.com/]', 'NaiveSound'))
};

//
// App window
//
var App = {
  view: (c) =>
    m('.app',
      m(MainSection, {tab: c.attrs.tab, glitch: c.attrs.glitch}),
      m(Toolbar, {tab: c.attrs.tab, glitch: c.attrs.glitch}))
};
var MainSection = {
  view: (c) =>
    m('.main-section',
      m(Header, {glitch: c.attrs.glitch}),
      m(((tab) => {
        switch (tab()) {
          case 'help': return Help;
          case 'library': return Library;
          case 'editor':
          default:
            return Editor;
        }
      })(c.attrs.tab), {glitch: c.attrs.glitch}))
};
var Header = {
  view: (c) =>
    m('.header',
      m('.title', '#glitch'),
      m(ErrorIcon, {glitch: c.attrs.glitch}),
      m(Visualizer, {glitch: c.attrs.glitch}))
};
var ErrorIcon = {
  view: (c) => {
    const style = { visibility: (c.attrs.glitch.error ? 'visible' : 'hidden') };
    return m('i.error-icon.material-icons.md-36', {style:style}, 'error');
  }
};

//
// Visualizer canvas
//
var Visualizer = {
  oncreate: function(c) {
    c.wrapper = c.dom;
    c.context = c.dom.children[0].getContext('2d');
    c.f = new Uint8Array(c.attrs.glitch.analyser.frequencyBinCount);
    c.t = new Uint8Array(c.attrs.glitch.analyser.frequencyBinCount);
    c.onresize = () => {
      c.state.width = c.wrapper.offsetWidth;
      c.state.height = c.wrapper.offsetHeight;
      m.redraw(true);
    };
    c.draw = () => {
      requestAnimationFrame(c.draw);
      c.context.fillStyle = GRAY;
      c.context.fillRect(0, 0, c.state.width, c.state.height);
      c.drawFFT(c.state.width, c.state.height);
      c.drawWaveForm(c.state.width, c.state.height);
    };
    c.drawWaveForm = () => {
      c.attrs.glitch.analyser.getByteTimeDomainData(c.t);
      let x = 0;
      c.context.beginPath();
      c.context.lineWidth = 2;
      c.context.strokeStyle = GREEN;
      const sliceWidth = c.state.width / c.t.length;
      for (let i = 0; i < c.t.length; i++) {
        const value = c.t[i] / 256;
        const y = c.state.height * 0.5 - (c.state.height * 0.45 * (value - 0.5));
        if (i === 0) {
          c.context.moveTo(x, y);
        } else {
          c.context.lineTo(x, y);
        }
        x += sliceWidth;
      }
      c.context.stroke();
    };
    c.drawFFT = () => {
      c.attrs.glitch.analyser.getByteFrequencyData(c.f);
      let x = 0;
      let v = 0;
      const sliceWidth = c.state.width / c.f.length;
      for (let i = 0; i < c.f.length; i++) {
        if (i % 10 === 0) {
          const y = (v * c.state.height * 0.45);
          c.context.fillStyle = PINK;
          c.context.fillRect(x, c.state.height / 2 - y / 20, 5 * sliceWidth, y / 10);
          v = 0;
        }
        v = v + c.f[i] / 256.0;
        x += sliceWidth;
      }
    };
    window.addEventListener('resize', c.onresize);
    c.onresize();
    c.draw();
  },
  onremove: function(c) {
    window.removeEventListener('resize', c.onresize);
  },
  view: (c) => {
    return m('div', {style: {flex: 1}},
      m('canvas', {
        width: c.state.width,
        height: c.state.height,
        style: {
          width: '100%',
          height: c.state.height,
        },
      }))
  }
};

//
// Toolbar layout
//
var Toolbar = {
  oninit: (c) => {
    c.state.isSaving = false;
    c.state.save = () => {
      c.state.isSaving = true;
      c.attrs.glitch.save(() => {
        c.state.isSaving = false;
        m.redraw();
      });
    };
  },
  view: (c) => {
    return m('.toolbar',
      (c.attrs.glitch.playing ?
        m(IconButton, {icon: 'pause', active: true, title: 'pause',
          onclick: () => c.attrs.glitch.stop()}) :
        m(IconButton, {icon: 'play_arrow', active: true, title: 'play',
          onclick: () => c.attrs.glitch.play()})),
      m(IconButton, {icon: 'code', active: (c.attrs.tab() == 'editor'), title: 'edit',
        onclick: () => c.attrs.tab('editor')}),
      m(IconButton, {icon: 'queue_music', active: (c.attrs.tab() == 'library'), title: 'examples',
        onclick: () => c.attrs.tab('library')}),
      m(IconButton, {icon: 'help_outline', active: (c.attrs.tab() == 'help'), title: 'help',
        onclick: () => c.attrs.tab('help')}),
      m('div', {style: {flex: 1}}),
      m(IconButton, {icon: 'file_download', active: !c.state.isSaving, title: 'save WAV file',
        onclick: () => c.state.save()}))
  }
};
var IconButton = {
  view: (c) => {
    const opacity = c.attrs.active ? 1 : 0.3;
    return m('.icon-button', Object.assign({}, c.attrs, {style: {opacity}}),
      m('i.material-icons.md-36', {style: {color: c.attrs.color}}, c.attrs.icon));
  }
};

//
// Editor textarea
//
var Editor = {
  controller: function(args) {
    this.expr = function(s) {
      if (s !== undefined) {
        args.glitch.compile(s);
      }
      return expr;
    };
  },
  view: (c) =>
    m('textarea.editor[autoComplete=off][autoCorrect=off][autoCapitalize=off][spellCheck=false]', {
      oninput: m.withAttr('value', (expr) => c.attrs.glitch.compile(expr)),
      value: c.attrs.glitch.userinput,
    })
};

//
// Library with examples
//
const EXAMPLES = [
  { f: 'byte(t*(42&t>>10))' },
  { f: 'byte((t*5&t>>7)|(t*3&t>>10))' },
  { f: 'byte(t*((t>>12|t>>8)&63&t>>4))' },
  { f: 'byte((t*((t>>9|t>>13)&15))&129)' },
  { f: 'byte((t*9&t>>4|t*5&t>>7|t*3&t/1024)-1)' },
  { f: 'byte((t>>6|t|t>>(t>>16))*10+((t>>11)&7))' },
  { f: 'byte((t>>6|t<<1)+(t>>5|t<<3|t>>3)|t>>2|t<<1)' },
  { f: 'byte((t*((3+(1^t>>10&5))*(5+(3&t>>14))))>>(t>>8&3))' },
  { f: 'byte(t*(t>>((t&4096)&&((t*t)/4096)||(t/4096)))|(t<<(t/256))|(t>>4))' },
  { f: 'byte((t*t/256)&(t>>((t/1024)%16))^t%64*(828188282217>>(t>>9&30)&t%32)*t>>18)' },
];

var Library = {
  ellipsisWidth: 0,
  oncreate: function(c) {
    var el = c.dom;
    c.state.onresize = () => {
      c.state.height = el.offsetHeight;
      c.state.ellipsisWidth = el.offsetWidth * 0.9;
      setTimeout(() => m.redraw());
    };
    c.state.onresize();
    window.addEventListener('resize', c.state.onresize);
  },
  onremove: function(c) {
    window.removeEventListener('resize', c.state.onresize);
  },
  view: (c) =>
    m('.examples',
      m('div', {style: {height: (c.state.ellipsisWidth === 0 ? 0 : `${c.state.height}px`)}},
        EXAMPLES.map((example) =>
          m('a', {
            key: example.f,
            expr: example.f,
            style: {
              cursor: 'pointer',
              color: (c.attrs.glitch.expr === example.f ? PINK : YELLOW),
              width: `${c.state.ellipsisWidth}px`,
            },
            onclick: (e) => {
              c.attrs.glitch.compile(example.f);
              c.attrs.glitch.play();
            },
          }, m.trust(`${c.attrs.glitch.expr === example.f ? '\u25b6' : '&nbsp'} ${example.f}`)))))
};

//
// Help text
//

const HELP = document.getElementById('help').innerHTML;
var Help = {
  oncreate: function(c) {
    function unescape(s) {
      const e = document.createElement('div');
      e.innerHTML = s;
      return e.childNodes.length === 0 ? '' : e.childNodes[0].nodeValue;
    }
    const links = c.dom.querySelectorAll('a');
    for (let i = 0; i < links.length; i++) {
      const a = links[i];
      a.onclick = (e) => {
        e.preventDefault();
        c.attrs.glitch.compile(unescape(a.innerHTML));
        c.attrs.glitch.play();
        m.redraw();
      };
    }
  },
  view: (c) =>
    m('.help',
      m('.help-contents', m.trust(HELP)))
};

//
// Glitch core
//
class Glitch {
  constructor() {
    if (typeof window !== 'undefined') {
      const audioContext = new window.AudioContext();
      _glitch_sample_rate(audioContext.sampleRate);

      if (navigator.requestMIDIAccess) {
        navigator.requestMIDIAccess().then((midi) => {
          const enumerate = () => {
            let inputs = midi.inputs.values();
            for (let input = inputs.next(); input && !input.done; input = inputs.next()) {
              input.value.onmidimessage = (m) => {
                Module.ccall('glitch_midi', 'number', ['number', 'number', 'number'],
                                     [this.g, m.data[0], m.data[1], m.data[2]]);
              };
            }
          };
          enumerate();
          midi.onstatechange = enumerate;
        });
      }


      const AUDIO_BUFFER_SIZE = 1024;
      const pcm = audioContext.createScriptProcessor(AUDIO_BUFFER_SIZE, 0, 1);
      const analyser = audioContext.createAnalyser();

      analyser.fftSize = 2048;
      analyser.smoothingTimeConstant = 0;
      analyser.connect(audioContext.destination);
      pcm.onaudioprocess = undefined;

      this.analyser = analyser;
      this.pcm = pcm;
    } else {
      this.sampleRate = 48000;
      this.analyser = {};
    }

    this.g = _glitch_create();
    this.playing = false;
    this.userinput = '';
    this.worker = new GlitchWorker();
  }
  compile(expr) {
    this.userinput = expr;
    var r = Module.ccall('glitch_compile', 'number', ['number', 'string', 'number'], [this.g, expr, expr.length]);
    if (r === 0) {
      this.expr = expr;
      this.error = undefined;
      window.location.hash = encodeURIComponent(expr);
      localStorage.setItem('expr', expr);
    } else {
      // TODO: debounce error
      this.error = true;
    }
  }
  xy(x, y) {
    _glitch_xy(this.g, x, y);
  }
  play() {
    if (!this.pcm.onaudioprocess) {
      this.pcm.connect(this.analyser);
      this.pcm.onaudioprocess = this.audioCallback.bind(this);
      this.playing = true;
    }
  }
  stop() {
    if (this.pcm.onaudioprocess) {
      this.pcm.disconnect();
      this.pcm.onaudioprocess = undefined;
      this.playing = false;
    }
  }
  audioCallback(e) {
    const buffer = e.outputBuffer.getChannelData(0);
    for (let i = 0; i < buffer.length; i++) {
      buffer[i] = _glitch_eval(this.g);
    }
  }
  save(cb) {
    this.worker.save(this.userinput, cb);
  }
}

class GlitchWorker {
  prepareWorker(cb) {
    if (!this.worker) {
      m.request({
        method: 'GET',
        url: '/glitchcore.js',
        deserialize: (data) => data,
      }).then((res) => {
        const glitchCoreJS = res;
        // Web worker can't access window.location, so we hardcode it in worker script
        const prefix = location.protocol + '//' + location.host + '/';
        function wav(sec, sampleRate, f) {
          const len = sec * sampleRate;
          const w = new Int16Array(len + 23);
          w[0] = 0x4952; // "RI"
          w[1] = 0x4646; // "FF"
          w[2] = (2 * len + 15) & 0x0000ffff; // RIFF size
          w[3] = ((2 * len + 15) & 0xffff0000) >> 16; // RIFF size
          w[4] = 0x4157; // "WA"
          w[5] = 0x4556; // "VE"
          w[6] = 0x6d66; // "fm"
          w[7] = 0x2074; // "t "
          w[8] = 0x0012; // fmt chunksize: 18
          w[9] = 0x0000; //
          w[10] = 0x0001; // format tag : 1
          w[11] = 1;     // channels: 1
          w[12] = sampleRate & 0x0000ffff; // sample per sec
          w[13] = (sampleRate & 0xffff0000) >> 16; // sample per sec
          w[14] = (2 * sampleRate) & 0x0000ffff; // byte per sec
          w[15] = ((2 * sampleRate) & 0xffff0000) >> 16; // byte per sec
          w[16] = 0x0004; // block align
          w[17] = 0x0010; // bit per sample
          w[18] = 0x0000; // cb size
          w[19] = 0x6164; // "da"
          w[20] = 0x6174; // "ta"
          w[21] = (2 * len) & 0x0000ffff; // data size[byte]
          w[22] = ((2 * len) & 0xffff0000) >> 16; // data size[byte]
          for (let i = 0; i < len; i++) {
            w[i + 23] = Math.round(f(i) * (1 << 15));
          }
          return w.buffer;
        };
        const glitchExportFunc = function(e) {
          Module['onRuntimeInitialized'] = function() {
            let buffer;
            const g = _glitch_create();
            const expr = e.data.expr;
            const r = Module.ccall('glitch_compile', 'number',
            ['number', 'string', 'number'], [g, expr, expr.length]);
            if (r === 0) {
              buffer = wav(180, 48000, () => _glitch_eval(g));
            }
            _glitch_destroy(g);
            self.postMessage({
              buffer: buffer,
            });
          }
        };
        this.worker = new Worker(window.URL.createObjectURL(new Blob([
          `(function() {
            var Module = {};
            Module['memoryInitializerPrefixURL'] = '${prefix}';
            ${glitchCoreJS};
            ${wav.toString()};
            self.onmessage = ${glitchExportFunc.toString()}
          })()`
        ], {type: "text/javascript"})));
        cb();
      });
    } else {
      cb();
    }
  }
  saveFile(name, data, contentType) {
    const blob = new Blob([data], { type: contentType });
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement('a');
    document.body.appendChild(a);
    a.style = 'display: none';
    a.href = url;
    a.download = name;
    a.click();
    setTimeout(() => {
      document.body.removeChild(a);
      window.URL.revokeObjectURL(url);
    }, 100);
  }
  save(expr, cb) {
    this.prepareWorker(() => {
      this.worker.onmessage = (e) => {
        this.saveFile('glitch.wav', e.data.buffer, 'audio/wav');
        cb();
      };
      this.worker.postMessage({expr: expr, duration: 30});
    });
  }
}

//
// Main part: create glitch audio player, handle global events, render layout
//
Module['onRuntimeInitialized'] = function() {
  var glitch = new Glitch();
  const DEFAULT_SONG = 't*(42&t>>10)';

  if (window.location.hash) {
    glitch.compile(decodeURIComponent(window.location.hash.substring(1)));
  } else if (localStorage.getItem('expr')) {
    glitch.compile(localStorage.getItem('expr'));
  } else {
    glitch.compile(DEFAULT_SONG);
  }
  if (window.location.search === '?play') {
    glitch.play();
  }

  document.onmousemove = (e) => {
    glitch.xy(e.pageX / window.innerWidth, e.pageY / window.innerHeight);
  };

  document.onkeydown = (e) => {
    if (e.keyCode === 13 && e.ctrlKey) {
      if (glitch.playing) {
        glitch.stop();
      } else {
        glitch.play();
      }
      m.redraw();
    }
  };

  var tab = function(v) {
    if (v !== undefined) {
      this.tab = v;
    }
    return this.tab;
  }.bind({});

  tab('editor');

  m.mount(document.body, {view: () => m(Layout, {tab: tab, glitch: glitch})});
};
