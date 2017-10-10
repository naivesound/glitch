package main

import (
	"io/ioutil"
	"math"
	"path/filepath"
	"sort"
	"strings"

	"github.com/naivesound/glitch/core"
)

type sampleLoader struct {
	samples map[string][][]float32
}

func (loader *sampleLoader) dir() string {
	return "samples"
}

func (loader *sampleLoader) poll() {
	loader.samples = map[string][][]float32{}
	for sample, variants := range loader.scan() {
		loader.samples[sample] = make([][]float32, len(variants), len(variants))
		for i, variant := range variants {
			loader.samples[sample][i] = loader.read(variant)
		}
		core.AddSample(sample)
	}
}

func (loader *sampleLoader) read(name string) []float32 {
	b, err := ioutil.ReadFile(name)
	if err != nil {
		return nil
	}
	f := []float32{}
	b = b[0x80:]
	for i := 0; i+1 < len(b); i = i + 2 {
		x := float32(int(b[i])+int(int8(b[i+1]))*256) / float32(0x8000)
		f = append(f, x)
	}
	return f
}

func (loader *sampleLoader) scan() (samples map[string][]string) {
	samples = map[string][]string{}
	files, err := ioutil.ReadDir(loader.dir())
	if err != nil {
		return samples
	}

	for _, file := range files {
		if file.IsDir() {
			name := file.Name()
			wavs, err := ioutil.ReadDir(filepath.Join(loader.dir(), name))
			if err != nil {
				continue
			}
			variants := []string{}
			for _, wav := range wavs {
				if !wav.IsDir() && strings.ToLower(filepath.Ext(wav.Name())) == ".wav" {
					variants = append(variants, filepath.Join(loader.dir(), name, wav.Name()))
				}
			}
			sort.Strings(variants)
			samples[name] = variants
		}
	}
	return samples
}

func (loader *sampleLoader) LoadSample(name string, variant, frame int) float32 {
	samples, ok := loader.samples[name]
	if !ok {
		return float32(math.NaN())
	}
	if variant >= 0 && variant < len(samples) && frame >= 0 && frame < len(samples[variant]) {
		return loader.samples[name][variant][frame]
	}
	return float32(math.NaN())
}
