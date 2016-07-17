"use strict";

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
  controller: function() {
    this.onresize = () => {
      this.fullscreen = (window.innerWidth < 800 || window.innerHeight < 500);
      m.redraw();
    };
    this.onunload = () => {
      window.removeEventListener('resize', this.onresize);
    };
    this.onresize();
    window.addEventListener('resize', this.onresize);
  },
  view: (c, args) => {
    let app = m(App, {tab: args.tab, glitch: args.glitch});
    if (!c.fullscreen) {
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
  view: (c, args) =>
    m('.app',
      m(MainSection, {tab: args.tab, glitch: args.glitch}),
      m(Toolbar, {tab: args.tab, glitch: args.glitch}))
};
var MainSection = {
  view: (c, args) =>
    m('.main-section',
      m(Header, {glitch: args.glitch}),
      m(((tab) => {
        switch (tab) {
          case 'help': return Help;
          case 'library': return Library;
          case 'editor':
          default:
            return Editor;
        }
      })(args.tab()), {glitch: args.glitch}))
};
var Header = {
  view: (c, args) =>
    m('.header',
      m('.title', '#glitch'),
      m(ErrorIcon, {glitch: args.glitch}),
      m(Visualizer, {glitch: args.glitch}))
};
var ErrorIcon = {
  view: (c, args) => {
    const style = { visibility: (args.glitch.error ? 'visible' : 'hidden') };
    return m('i.error-icon.material-icons.md-36', {style:style}, 'error');
  }
};

//
// Visualizer canvas
//
var Visualizer = {
  controller: function(args) {
    this.draw = () => {
      requestAnimationFrame(this.draw);
      this.context.fillStyle = GRAY;
      this.context.fillRect(0, 0, this.width, this.height);
      this.drawFFT(this.width, this.height);
      this.drawWaveForm(this.width, this.height);
    };
    this.drawFFT = () => {
      args.glitch.analyser.getByteFrequencyData(this.f);
      let x = 0;
      let v = 0;
      const sliceWidth = this.width / this.f.length;
      for (let i = 0; i < this.f.length; i++) {
        if (i % 10 === 0) {
          const y = (v * this.height * 0.45);
          this.context.fillStyle = PINK;
          this.context.fillRect(x, this.height / 2 - y / 20, 5 * sliceWidth, y / 10);
          v = 0;
        }
        v = v + this.f[i] / 256.0;
        x += sliceWidth;
      }
    };
    this.drawWaveForm = () => {
      args.glitch.analyser.getByteTimeDomainData(this.t);
      let x = 0;
      this.context.beginPath();
      this.context.lineWidth = 2;
      this.context.strokeStyle = GREEN;
      const sliceWidth = this.width / this.t.length;
      for (let i = 0; i < this.t.length; i++) {
        const value = this.t[i] / 256;
        const y = this.height * 0.5 - (this.height * 0.45 * (value - 0.5));
        if (i === 0) {
          this.context.moveTo(x, y);
        } else {
          this.context.lineTo(x, y);
        }
        x += sliceWidth;
      }
      this.context.stroke();

    };
    this.onresize = () => {
      this.width = this.wrapper.offsetWidth;
      this.height = this.wrapper.offsetHeight;
      m.redraw();
    };
    this.config = (el, isinit) => {
      if (!isinit) {
        this.wrapper = el;
        this.context = el.children[0].getContext('2d');
        this.onresize();
        this.draw();
      }
    };
    this.onunload = () => {
      window.removeEventListener('resize', this.onresize);
    };
    this.f = new Uint8Array(args.glitch.analyser.frequencyBinCount);
    this.t = new Uint8Array(args.glitch.analyser.frequencyBinCount);

    window.addEventListener('resize', this.onresize);
  },
  view: (c, args) =>
    m('div', {config: c.config, style: {flex: 1}},
      m('canvas', {
        width: c.width,
        height: c.height,
        style: {
          width: '100%',
          height: c.height,
        },
      }))
};

//
// Toolbar layout
//
var Toolbar = {
  controller: function(args) {
    this.saving = m.prop(false);
    this.save = () => {
      this.saving(true);
      args.glitch.save(() => {
        this.saving(false);
        m.redraw();
      });
    };
  },
  view: (c, args) =>
    m('.toolbar',
      (args.glitch.playing ?
        m(IconButton, {icon: 'pause', active: true, title: 'pause',
          onclick: () => args.glitch.stop()}) :
        m(IconButton, {icon: 'play_arrow', active: true, title: 'play',
          onclick: () => args.glitch.play()})),
      m(IconButton, {icon: 'code', active: (args.tab() == 'editor'), title: 'edit',
        onclick: () => args.tab('editor')}),
      m(IconButton, {icon: 'queue_music', active: (args.tab() == 'library'), title: 'examples',
        onclick: () => args.tab('library')}),
      m(IconButton, {icon: 'help_outline', active: (args.tab() == 'help'), title: 'help',
        onclick: () => args.tab('help')}),
      m('div', {style: {flex: 1}}),
      m(IconButton, {icon: 'file_download', active: !c.saving(), title: 'save WAV file',
        onclick: () => c.save()}))
};
var IconButton = {
  view: (c, args) => {
    const opacity = args.active ? 1 : 0.3;
    return m('.icon-button', Object.assign({}, args, {style: {opacity}}),
      m('i.material-icons.md-36', {style: {color: args.color}}, args.icon));
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
  view: (c, args) =>
    m('textarea.editor[autoComplete=off][autoCorrect=off][autoCapitalize=off][spellCheck=false]', {
      oninput: m.withAttr('value', (expr) => args.glitch.compile(expr)),
      value: args.glitch.userinput(),
    })
};

//
// Library with examples
//
const EXAMPLES = [
  { f: 't*(42&t>>10)'},
  { f: 't>>6&1&&t>>5||-t>>4'},
  { f: '(t*5&t>>7)|(t*3&t>>10)'},
  { f: '(t*(t>>5|t>>8))>>(t>>16)'},
  { f: 't*((t>>12|t>>8)&63&t>>4)'},
  { f: 't*5&(t>>7)|t*3&(t*4>>10)'},
  { f: '(t*((t>>9|t>>13)&15))&129'},
  { f: 't*(((t>>9)^((t>>9)-1)^1)%13)'},
  { f: 't*(51864>>(t>>9&14)&15)|t>>8'},
  { f: 't&(s(t&t&3)*t>>5)/(t>>3&t>>6)'},
  { f: '(t*9&t>>4|t*5&t>>7|t*3&t/1024)-1'},
  { f: '(t>>6|t|t>>(t>>16))*10+((t>>11)&7)'},
  { f: '(t>>6|t<<1)+(t>>5|t<<3|t>>3)|t>>2|t<<1'},
  { f: '(t*((3+(1^t>>10&5))*(5+(3&t>>14))))>>(t>>8&3)'},
  { f: 't*(t>>((t&4096)&&((t*t)/4096)||(t/4096)))|(t<<(t/256))|(t>>4)'},
  { f: '((t&4096)&&((t*(t^t%255)|(t>>4))>>1)||(t>>3)|((t&8192)&&t<<2||t))'},
  { f: '(t*t/256)&(t>>((t/1024)%16))^t%64*(828188282217>>(t>>9&30)&t%32)*t>>18'},
];
var Library = {
  controller: function(args) {
    this.config = (el, isinit, context) => {
      this.el = el;
      if (!context.init) {
        context.init = true;
        this.onresize();
      }
    };
    this.ellipsisWidth = 0;
    this.onresize = () => {
      this.height = this.el.offsetHeight;
      this.ellipsisWidth = this.el.offsetWidth * 0.9;
      setTimeout(() => m.redraw());
    };
    this.onunload = () => {
      window.removeEventListener('resize', this.onresize);
    };
    window.addEventListener('resize', this.onresize);
  },
  view: (c, args) =>
    m('.examples', {config: c.config, onunload: c.onunload}, // workaround for mithril bug #1098
      m('div', {style: {height: (c.ellipsisWidth === 0 ? 0 : `${c.height}px`)}},
        EXAMPLES.map((example) =>
          m('a', {
            key: example.f,
            expr: example.f,
            style: {
              cursor: 'pointer',
              color: (args.glitch.expr === example.f ? PINK : YELLOW),
              width: `${c.ellipsisWidth}px`,
            },
            onclick: (e) => {
              args.glitch.compile(example.f);
              args.glitch.play();
            },
          }, m.trust(`${args.glitch.expr === example.f ? '\u25b6' : '&nbsp'} ${example.f}`)))))
};

//
// Help text
//

const HELP = document.getElementById('help').innerHTML;
var Help = {
  controller: function(args) {
    function unescape(s) {
      const e = document.createElement('div');
      e.innerHTML = s;
      return e.childNodes.length === 0 ? '' : e.childNodes[0].nodeValue;
    }
    this.config = function(el, init) {
      if (el && !init) {
        const links = el.querySelectorAll('a');
        for (let i = 0; i < links.length; i++) {
          const a = links[i];
          a.onclick = (e) => {
            e.preventDefault();
            args.glitch.compile(unescape(a.innerHTML));
            args.glitch.play();
            m.redraw();
          };
        }
      }
    };
  },
  view: (c) =>
    m('.help', {config: c.config},
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

      const AUDIO_BUFFER_SIZE = 8192;
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
    this.userinput = m.prop('');
    this.worker = new GlitchWorker();
  }
  compile(expr) {
    this.userinput(expr);
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
    this.worker.save(this.userinput(), cb);
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
          console.log('export func');
          Module['onRuntimeInitialized'] = function() {
            console.log('memory initialized');
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

  m.mount(document.body, m(Layout, {tab: m.prop('editor'), glitch: glitch}));
};
