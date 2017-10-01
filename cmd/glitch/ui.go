package main

import (
	"bytes"
	"encoding/json"
	"io"
	"log"
	"mime"
	"net"
	"net/http"
	"path/filepath"

	"github.com/naivesound/glitch/core"
	"github.com/zserge/webview"
)

const (
	windowWidth  = 640
	windowHeight = 480
)

func startServer() string {
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		log.Fatal(err)
	}
	go func() {
		defer ln.Close()
		http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
			path := r.URL.Path
			if len(path) > 0 && path[0] == '/' {
				path = path[1:]
			}
			if path == "" {
				path = "index.html"
			}
			if bs, err := Asset(path); err != nil {
				w.WriteHeader(http.StatusNotFound)
			} else {
				w.Header().Add("Content-Type", mime.TypeByExtension(filepath.Ext(path)))
				io.Copy(w, bytes.NewBuffer(bs))
			}
		})
		log.Fatal(http.Serve(ln, nil))
	}()
	return "http://" + ln.Addr().String()
}

func handleRPC(g core.Glitch) webview.ExternalInvokeCallbackFunc {
	return func(w webview.WebView, data string) {
		m := map[string]string{}
		if err := json.Unmarshal([]byte(data), &m); err != nil {
			log.Println(data, err)
			return
		}
		switch m["cmd"] {
		case "init":
			w.Eval(`editor.setValue("bpm=120/4\n")`)
		case "changeText":
			g.Compile(m["text"])
		case "togglePlayback":
			paused = !paused
		}
	}
}

func uiLoop(g core.Glitch) {
	url := startServer()
	w := webview.New(webview.Settings{
		Width:     windowWidth,
		Height:    windowHeight,
		Resizable: true,
		Title:     "Glitch",
		URL:       url,
		ExternalInvokeCallback: handleRPC(g),
	})
	defer w.Exit()

	w.Run()
}
