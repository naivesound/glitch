package main

//go:generate go run -tags gen gen.go

import (
	"log"
	"time"

	"github.com/naivesound/glitch/audio"
	"github.com/naivesound/glitch/core"
)

const (
	SampleRate  = 48000.0
	AudioBufSz  = 512
	NumChannels = 1
)

var paused = false

func main() {
	core.Init(SampleRate, uint64(time.Now().UnixNano()))
	g := core.NewGlitch()
	defer g.Destroy()

	if err := g.Compile(``); err != nil {
		log.Fatal(err)
	}

	m, err := audio.NewMIDI(func(msg []byte) {
		g.MIDI(msg)
	})
	if err != nil {
		log.Fatal(err)
	}
	defer m.Destroy()

	a, err := audio.NewAudio(func(in, out []float32, sr, frames, inChannels, outChannels int) {
		samples := out
		if !paused {
			g.Fill(samples, len(samples)/outChannels, outChannels)
		} else {
			for i := 0; i < len(samples); i++ {
				samples[i] = 0
			}
		}
	})
	if err != nil {
		log.Fatal(err)
	}
	defer a.Destroy()

	a.Open(4, 48000, 512, 0, 2)

	uiLoop(g)
}
