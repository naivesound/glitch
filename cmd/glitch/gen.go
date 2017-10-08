// +build gen

package main

import (
	"log"
	"os"
	"path/filepath"
	"regexp"

	"github.com/jteeuwen/go-bindata"
)

func main() {
	cfg := bindata.NewConfig()
	cfg.Package = "main"
	cfg.Output = "assets.go"
	wd, err := os.Getwd()
	if err != nil {
		log.Fatal(err)
	}
	uiDir, err := filepath.Abs(filepath.Clean(filepath.Join(wd, "../../ui")))
	if err != nil {
		log.Fatal(err)
	}
	cfg.Prefix = uiDir
	cfg.Input = []bindata.InputConfig{{Path: uiDir, Recursive: true}}
	cfg.Ignore = []*regexp.Regexp{
		// Favicons are not used
		regexp.MustCompile(`favicon\.png`),
		regexp.MustCompile(`glitch180x180\.png`),
		regexp.MustCompile(`glitch192x192\.png`),
		// WebAssembly and asm.js files are not used in desktop variant
		regexp.MustCompile(`glitch-asm\.js`),
		regexp.MustCompile(`glitch-asm\.js\.mem`),
		regexp.MustCompile(`glitch-loader\.js`),
		regexp.MustCompile(`glitch\.wasm`),
	}
	if err := bindata.Translate(cfg); err != nil {
		log.Fatal("bindata erro:", err)
	}
}
