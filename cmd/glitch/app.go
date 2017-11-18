package main

import (
	"io/ioutil"
	"log"
	"os"
	"path/filepath"
	"time"

	"github.com/naivesound/glitch/audio"
	"github.com/naivesound/glitch/core"
	"github.com/zserge/webview"
)

type App struct {
	glitch  core.Glitch
	audio   audio.Audio
	midi    audio.MIDI
	notify  chan struct{}
	webview webview.WebView

	sync func()

	*Config

	IsPlaying    bool               `json:"isPlaying"`
	Text         string             `json:"text"`
	Error        error              `json:"error"`
	AudioDevices []audio.Device     `json:"audioDevices"`
	MIDIDevices  []audio.MIDIDevice `json:"midiDevices"`
}

func NewApp(config *Config) (app *App, err error) {
	app = &App{Config: config}

	loader := &sampleLoader{}
	go loader.poll()
	core.Loader = loader
	core.Init(config.SampleRate, uint64(time.Now().UnixNano()))

	app.glitch = core.NewGlitch()

	app.notify = make(chan struct{}, 256)
	if app.midi, err = audio.NewMIDI(app.notify, func(msg []byte) {
		app.glitch.MIDI(msg)
	}); err != nil {
		app.Destroy()
		return nil, err
	}

	if app.audio, err = audio.NewAudio(app.notify, func(in, out []float32, sr, frames, inChannels, outChannels int) {
		samples := out
		if app.IsPlaying {
			app.glitch.Fill(samples, len(samples)/outChannels, outChannels)
		} else {
			for i := 0; i < len(samples); i++ {
				samples[i] = 0
			}
		}
	}); err != nil {
		app.Destroy()
		return nil, err
	}

	app.AudioDevices = app.audio.Devices()
	app.MIDIDevices = app.midi.Devices()

	url := startServer()
	app.webview = webview.New(webview.Settings{
		Width:     windowWidth,
		Height:    windowHeight,
		Resizable: true,
		URL:       url,
		ExternalInvokeCallback: func(w webview.WebView, data string) {
			if data == "__app_js_loaded__" {
				app.sync, err = app.webview.Bind("app", app)
				if err != nil {
					log.Println(err)
				} else {
					w.Eval(`init()`)
					go func() {
						for range app.notify {
							app.webview.Dispatch(func() {
								app.AudioDevice = app.audio.Current().ID
								app.AudioDevices = app.audio.Devices()
								app.MIDIDevices = app.midi.Devices()
								app.sync()
							})
						}
					}()
				}
			} else {
				log.Println("unhandled external invoke:", data)
			}
		},
	})

	for midi, connect := range config.MIDI {
		app.SelectMIDI(midi, connect)
	}
	app.SelectAudio(config.AudioDevice, config.SampleRate, config.BufferSize)
	app.Load(config.Filename)
	return app, nil
}

func (app *App) Run() {
	app.webview.Run()
}

func (app *App) Destroy() {
	close(app.notify)
	if app.webview != nil {
		app.webview.Exit()
	}
	if app.audio != nil {
		app.audio.Destroy()
	}
	if app.midi != nil {
		app.midi.Destroy()
	}
	if app.glitch != nil {
		app.glitch.Destroy()
	}
}

func (app *App) NewFile() {
	if name := app.webview.Dialog(webview.DialogTypeSave, 0, "New file...", ""); name != "" {
		app.Load(name)
	}
}

func (app *App) LoadFile() {
	if name := app.webview.Dialog(webview.DialogTypeOpen, 0, "Open file...", ""); name != "" {
		app.Load(name)
	}
}

func (app *App) Load(path string) {
	app.glitch.Reset()
	app.Filename = path
	os.MkdirAll(filepath.Dir(path), 0755)
	app.webview.SetTitle("Glitch - " + path)
	if b, err := ioutil.ReadFile(path); err == nil {
		app.Text = string(b)
	} else {
		app.Text = ""
	}
	app.Error = app.glitch.Compile(app.Text)
	app.Config.Save()
}

func (app *App) SetVar(name string, value float32) {
	app.glitch.Set(name, value)
}

func (app *App) ChangeText(text string) {
	app.Text = text
	app.Error = app.glitch.Compile(app.Text)
	ioutil.WriteFile(app.Filename, []byte(app.Text), 0644)
}

func (app *App) SelectAudio(id, sampleRate, bufsz int) {
	devices := app.audio.Devices()
	if id < 0 || id >= len(devices) {
		return
	}
	in := 0
	out := 1
	if devices[id].InputChannels == 0 {
		in = 0
	}
	if devices[id].OutputChannels == 0 {
		out = 0
	}
	app.audio.Open(id, sampleRate, bufsz, in, out)
	app.AudioDevice = id
	app.BufferSize = bufsz
	app.SampleRate = sampleRate
	core.Init(sampleRate, uint64(time.Now().UnixNano()))
	app.Config.Save()
}

func (app *App) SelectMIDI(name string, connected bool) {
	app.Config.MIDI[name] = connected
	app.midi.SetConnect(name, connected)
	app.Config.Save()
}

func (app *App) TogglePlayback() {
	app.IsPlaying = !app.IsPlaying
}

func (app *App) Stop() {
	app.glitch.Reset()
	app.glitch.Compile(app.Text)
	app.IsPlaying = false
}
