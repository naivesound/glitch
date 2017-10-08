package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"mime"
	"net"
	"net/http"
	"os"
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

var state = struct {
	Filename  string `json:"filename"`
	IsPlaying bool   `json:"isPlaying"`
	Text      string `json:"text"`
	Error     error  `json:"error"`
}{}

func handleRPC(g core.Glitch) webview.ExternalInvokeCallbackFunc {
	return func(w webview.WebView, data string) {
		m := map[string]interface{}{}
		if err := json.Unmarshal([]byte(data), &m); err != nil {
			log.Println(data, err)
			return
		}
		switch m["cmd"].(string) {
		case "nop":
		case "init":
		case "newFile":
			name := w.Dialog(webview.DialogTypeSave, 0, "New file...", "")
			if name != "" {
				state.Filename = name
				state.Text = ""
				state.Error = nil
			}
		case "loadFile":
			name := w.Dialog(webview.DialogTypeOpen, 0, "Open file...", "")
			if name != "" {
				if b, err := ioutil.ReadFile(name); err == nil {
					state.Filename = name
					state.Text = string(b)
					state.Error = g.Compile(state.Text)
					w.SetTitle("Glitch - " + state.Filename)
				}
			}
		case "changeText":
			state.Text = m["text"].(string)
			state.Error = g.Compile(state.Text)
			ioutil.WriteFile(state.Filename, []byte(state.Text), 0644)
		case "setVar":
			g.Set(m["name"].(string), float32(m["value"].(float64)))
		case "stop":
			g.Reset()
			g.Compile(state.Text)
			state.IsPlaying = false
		case "togglePlayback":
			state.IsPlaying = !state.IsPlaying
		default:
			log.Println("unknown command", m["cmd"])
		}
		b, err := json.Marshal(state)
		if err == nil {
			w.Eval(fmt.Sprintf(`window.rpc.render(%s)`, string(b)))
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

	state.Filename = filepath.Join(os.TempDir(), "glitch", "draft.glitch")
	os.MkdirAll(filepath.Dir(state.Filename), 0755)
	w.SetTitle("Glitch - " + state.Filename)
	if b, err := ioutil.ReadFile(state.Filename); err == nil {
		state.Text = string(b)
	} else {
		state.Text = "bpm = 120/4"
	}
	state.Error = g.Compile(state.Text)
	w.Run()
}
