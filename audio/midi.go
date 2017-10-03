package audio

import (
	"log"
	"sync"
	"time"

	"github.com/thestk/rtmidi/contrib/go/rtmidi"
)

type MIDICallback func(msg []byte)

type MIDIDevice struct {
	ID        int
	Name      string
	Connected bool
	midi      rtmidi.MIDIIn
}

type midi struct {
	sync.Mutex
	cb     MIDICallback
	wg     *sync.WaitGroup
	midi   rtmidi.MIDIIn
	inputs []MIDIDevice
	exitc  chan struct{}
}

func NewMIDI(cb MIDICallback) (*midi, error) {
	midiIn, err := rtmidi.NewMIDIInDefault()
	if err != nil {
		return nil, err
	}

	m := &midi{
		cb:    cb,
		wg:    &sync.WaitGroup{},
		midi:  midiIn,
		exitc: make(chan struct{}),
	}
	m.wg.Add(1)
	go m.poll()
	return m, nil
}

func (m *midi) Devices() []MIDIDevice {
	m.Lock()
	defer m.Unlock()
	return m.inputs
}

func (m *midi) Destroy() {
	close(m.exitc)
	m.wg.Wait()
	m.midi.Close()
}

func (m *midi) poll() {
	defer m.wg.Done()
	for {
		if err := m.pollOnce(); err != nil {
			log.Println(err)
		}
		select {
		case <-m.exitc:
			return
		case <-time.After(time.Second):
		}
	}
}

func (m *midi) pollOnce() error {
	m.Lock()
	defer m.Unlock()
	n, err := m.midi.PortCount()
	if err != nil {
		return err
	}
	names := make([]string, n, n)
	for i := 0; i < n; i++ {
		name, err := m.midi.PortName(i)
		if err == nil {
			names[i] = name
		}
	}
	inputs := []MIDIDevice{}
	for _, d := range m.inputs {
		if d.ID < len(names) && d.Name == names[d.ID] {
			inputs = append(inputs, d)
		} else {
			d.Connected = false
			d.midi.CancelCallback()
			d.midi.Close()
		}
	}
	m.inputs = inputs
	for i := 0; i < n; i++ {
		found := false
		// TODO: check if whitelisted
		for _, d := range m.inputs {
			if d.ID == i && d.Name == names[i] {
				found = true
				break
			}
		}
		if !found {
			if in, err := rtmidi.NewMIDIInDefault(); err != nil {
				return err
			} else if err := in.OpenPort(i, names[i]); err != nil {
				return err
			} else {
				d := MIDIDevice{ID: i, Name: names[i], Connected: true, midi: in}
				d.midi.SetCallback(func(_ rtmidi.MIDIIn, msg []byte, _ float64) {
					m.cb(msg)
				})
				m.inputs = append(m.inputs, d)
			}
		}
	}
	return nil
}
