package audio

import (
	"sync"
	"time"

	"github.com/naivesound/audio/rtaudio"
)

type Callback func(in, out []float32, sr, frames, inChannels, outChannels int)

type Device struct {
	Index             int
	Name              string
	SampleRates       []int
	DefaultSampleRate int
	InputChannels     int
	OutputChannels    int
}

type Audio interface {
	Devices() []Device
	Current() Device
	Open(i, sr, bufSz, inChans, outChans int)
	Destroy()
}

type rt struct {
	sync.Mutex
	audio         rtaudio.RtAudio
	cb            Callback
	currentID     int
	currentDevice Device
}

func NewAudio(cb Callback) (Audio, error) {
	audio, err := rtaudio.Create(rtaudio.APIUnspecified)
	if err != nil {
		return nil, err
	}
	rt := &rt{audio: audio, cb: cb, currentID: -1}
	return rt, nil
}

func (rt *rt) Destroy() {
	rt.Lock()
	defer rt.Unlock()
	if rt.currentID != -1 {
		rt.audio.Close()
		rt.currentID = -1
	}
	rt.audio.Destroy()
}

func (rt *rt) Devices() []Device {
	rt.Lock()
	defer rt.Unlock()
	devices := []Device{}
	infos, err := rt.audio.Devices()
	if err == nil {
		for i, info := range infos {
			devices = append(devices, Device{
				Index:             i,
				Name:              info.Name,
				OutputChannels:    info.NumOutputChannels,
				InputChannels:     info.NumInputChannels,
				SampleRates:       info.SampleRates,
				DefaultSampleRate: int(info.PreferredSampleRate),
			})
		}
	}
	return devices
}

func (rt *rt) Current() Device {
	rt.Lock()
	defer rt.Unlock()
	return rt.currentDevice
}

func (rt *rt) Open(id, sampleRate, bufSz int, inChans, outChans int) {
	rt.Lock()
	if rt.currentID != -1 {
		rt.audio.Close()
		rt.currentID = -1
	}
	rt.Unlock()
	var inParams, outParams *rtaudio.StreamParams
	devices := rt.Devices()
	if id < 0 || id >= len(devices) {
		return
	}
	if inChans > 0 {
		inParams = &rtaudio.StreamParams{
			DeviceID:     uint(id),
			NumChannels:  uint(inChans),
			FirstChannel: 0,
		}
	}
	if outChans > 0 {
		outParams = &rtaudio.StreamParams{
			DeviceID:     uint(id),
			NumChannels:  uint(outChans),
			FirstChannel: 0,
		}
	}
	err := rt.audio.Open(outParams, inParams, rtaudio.FormatFloat32,
		uint(sampleRate), uint(bufSz),
		func(out, in rtaudio.Buffer, dur time.Duration, status rtaudio.StreamStatus) int {
			if status == rtaudio.StatusOutputUnderflow {
				return 0
			}
			rt.cb(in.Float32(), out.Float32(), sampleRate, bufSz, inChans, outChans)
			return 0
		}, nil)
	if err == nil {
		rt.Lock()
		rt.currentID = id
		rt.currentDevice = devices[id]
		rt.audio.Start()
		rt.Unlock()
	}
}
