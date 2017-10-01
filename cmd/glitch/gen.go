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
		{Path: uiDir},
		{Path: filepath.Join(uiDir, "vendor/codemirror")},
		{Path: filepath.Join(uiDir, "vendor/roboto-mono")},
		{Path: filepath.Join(uiDir, "vendor/material-icons")},
	}
	if err := bindata.Translate(cfg); err != nil {
		log.Fatal(err)
	}
}
