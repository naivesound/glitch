package audio

import (
	"log"
	"sync"
	"time"

	"github.com/thestk/rtmidi/contrib/go/rtmidi"
)

type MIDICallback func(msg []byte)

type MIDIDevice struct {
	ID        int    `json:"id"`
	Name      string `json:"name"`
	Connected bool   `json:"connected"`
	midi      rtmidi.MIDIIn
}

type MIDI interface {
	SetAutoConnect(bool)
	AutoConnect() bool
	SetConnect(name string, connect bool)
	Devices() []MIDIDevice
	Destroy()
}

type midi struct {
	sync.Mutex
	cb          MIDICallback
	wg          *sync.WaitGroup
	midi        rtmidi.MIDIIn
	autoConnect bool
	connect     map[string]bool
	inputs      []*MIDIDevice
	configc     chan struct{}
	exitc       chan struct{}
	notify      chan<- struct{}
}

func NewMIDI(notify chan<- struct{}, cb MIDICallback) (MIDI, error) {
	midiIn, err := rtmidi.NewMIDIInDefault()
	if err != nil {
		return nil, err
	}

	m := &midi{
		cb:          cb,
		wg:          &sync.WaitGroup{},
		midi:        midiIn,
		notify:      notify,
		autoConnect: true,
		connect:     map[string]bool{},
		configc:     make(chan struct{}, 1),
		exitc:       make(chan struct{}),
	}
	m.wg.Add(1)
	go m.poll()
	return m, nil
}

func (m *midi) SetAutoConnect(connect bool) {
	m.Lock()
	defer m.Unlock()
	m.autoConnect = connect
	m.configc <- struct{}{}
}

func (m *midi) AutoConnect() bool {
	m.Lock()
	defer m.Unlock()
	return m.autoConnect
}

func (m *midi) SetConnect(name string, connect bool) {
	m.Lock()
	defer m.Unlock()
	m.connect[name] = connect
	m.configc <- struct{}{}
}

func (m *midi) Devices() (devices []MIDIDevice) {
	m.Lock()
	defer m.Unlock()
	for _, d := range m.inputs {
		devices = append(devices, MIDIDevice{ID: d.ID, Name: d.Name, Connected: d.Connected})
	}
	return devices
}

func (m *midi) Destroy() {
	close(m.exitc)
	close(m.configc)
	m.wg.Wait()
	m.midi.Close()
}

func (m *midi) shouldConnect(name string) bool {
	connect, ok := m.connect[name]
	return (connect && ok) || (!ok && m.autoConnect)
}

func (m *midi) openDevice(d *MIDIDevice) (err error) {
	if d.midi, err = rtmidi.NewMIDIInDefault(); err != nil {
		return err
	} else if err = d.midi.OpenPort(d.ID, d.Name); err != nil {
		return err
	} else {
		d.Connected = true
		d.midi.SetCallback(func(_ rtmidi.MIDIIn, msg []byte, _ float64) {
			m.cb(msg)
		})
	}
	return nil
}

func (m *midi) closeDevice(d *MIDIDevice) {
	d.Connected = false
	d.midi.CancelCallback()
	d.midi.Close()
}

func (m *midi) poll() {
	defer m.wg.Done()
	for {
		if changed, err := m.pollOnce(); err != nil {
			log.Println(err)
		} else if changed {
			m.notify <- struct{}{}
		}
		select {
		case <-m.exitc:
			return
		case <-m.configc:
		case <-time.After(time.Second):
		}
	}
}

func (m *midi) pollOnce() (changed bool, err error) {
	m.Lock()
	defer m.Unlock()

	// Get a list of currently connected devices
	n, err := m.midi.PortCount()
	if err != nil {
		return changed, err
	}
	names := make([]string, n, n)
	for i := 0; i < n; i++ {
		name, err := m.midi.PortName(i)
		if err == nil {
			names[i] = name
		}
	}
	inputs := []*MIDIDevice{}

	// Close and remove disconnected devices
	for _, d := range m.inputs {
		if d.ID < len(names) && d.Name == names[d.ID] {
			inputs = append(inputs, d)
		} else {
			m.closeDevice(d)
			changed = true
		}
	}

	// Add newly connected devices and change connection status if needed
	m.inputs = inputs
	for i := 0; i < n; i++ {
		var device *MIDIDevice
		for _, d := range m.inputs {
			if d.ID == i && d.Name == names[i] {
				device = d
				break
			}
		}

		if device != nil {
			if device.Connected && !m.shouldConnect(device.Name) {
				// Existing device should be disconnected
				m.closeDevice(device)
				changed = true
			} else if !device.Connected && m.shouldConnect(device.Name) {
				// Existing device should be connected
				if err := m.openDevice(device); err != nil {
					return changed, err
				}
				changed = true
			}
		} else {
			// New device should be connected
			device = &MIDIDevice{ID: i, Name: names[i], Connected: m.shouldConnect(names[i])}
			if device.Connected {
				if err := m.openDevice(device); err != nil {
					return changed, err
				}
				changed = true
			}
			m.inputs = append(m.inputs, device)
		}
	}
	return changed, nil
}
