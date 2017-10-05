// +build gen

package main

import (
	"log"
	"path/filepath"

	"github.com/jteeuwen/go-bindata"
)

func main() {
	cfg := bindata.NewConfig()
	cfg.Package = "main"
	cfg.Output = "assets.go"
	uiDir := "../../ui"
	cfg.Prefix = uiDir
	cfg.Input = []bindata.InputConfig{
		{Path: filepath.Join(uiDir, "index.html")},
		{Path: filepath.Join(uiDir, "styles.css")},
		{Path: filepath.Join(uiDir, "app.js")},
		{Path: filepath.Join(uiDir, "vendor/codemirror")},
		{Path: filepath.Join(uiDir, "vendor/roboto-mono")},
		{Path: filepath.Join(uiDir, "vendor/material-icons")},
		{Path: filepath.Join(uiDir, "vendor/picodom")},
	}
	if err := bindata.Translate(cfg); err != nil {
		log.Fatal(err)
	}
}
