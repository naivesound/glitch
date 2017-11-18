package main

//go:generate go run -tags gen gen.go

import (
	"encoding/json"
	"io/ioutil"
	"log"
	"os"
	"path/filepath"
	"runtime"
)

const (
	DefaultSampleRate = 44100.0
	DefaultBufSz      = 512
)

type Config struct {
	Filename    string          `json:"filename"`
	MIDI        map[string]bool `json:"midi"`
	AudioDevice int             `json:"audioDevice"`
	SampleRate  int             `json:"sampleRate"`
	BufferSize  int             `json:"bufferSize"`
}

var DefaultConfig = Config{
	Filename:    filepath.Join(os.TempDir(), "glitch", "draft.glitch"),
	MIDI:        map[string]bool{},
	AudioDevice: 0,
	SampleRate:  DefaultSampleRate,
	BufferSize:  DefaultBufSz,
}

func configPath() (path string) {
	switch runtime.GOOS {
	case "darwin":
		path = filepath.Join(os.Getenv("HOME"), "/Library/Application Support")
	case "windows":
		path = os.Getenv("APPDATA")
	default:
		if os.Getenv("XDG_CONFIG_HOME") != "" {
			path = os.Getenv("XDG_CONFIG_HOME")
		} else {
			path = filepath.Join(os.Getenv("HOME"), ".config")
		}
	}
	path = filepath.Join(path, "glitch", "config.json")
	os.MkdirAll(filepath.Dir(path), 0755)
	return path
}

func NewConfig() (c Config) {
	if b, err := ioutil.ReadFile(configPath()); err != nil {
		return DefaultConfig
	} else if err := json.Unmarshal(b, &c); err != nil {
		return DefaultConfig
	}
	return c
}

func (c *Config) Save() {
	if b, err := json.Marshal(c); err != nil {
		log.Println(err)
	} else if err := ioutil.WriteFile(configPath(), b, 0644); err != nil {
		log.Println(err)
	}
}

func main() {
	c := NewConfig()
	app, err := NewApp(&c)
	if err != nil {
		log.Fatal(err)
	}
	defer app.Destroy()
	app.Run()
}
