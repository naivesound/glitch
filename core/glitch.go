package core

/*
#cgo CFLAGS: -std=c99 -g -Wall -Wextra -pedantic -Wno-unused-parameter -Wno-unused-function
#cgo LDFLAGS: -lm -g

#include <stdlib.h>
#include "glitch.h"
*/
import "C"
import (
	"errors"
	"sync"
	"time"
	"unsafe"
)

func init() {
	// TODO: set sample loader
	Init(44100, uint64(time.Now().UnixNano()))
}

func Init(sr int, seed uint64) {
	C.glitch_init(C.int(sr), C.ulonglong(seed))
}

func AddSample(name string) bool {
	p := C.CString(name)
	defer C.free(unsafe.Pointer(p))
	return C.glitch_add_sample(p) == 0
}

func RemoveSample(name string) bool {
	p := C.CString(name)
	defer C.free(unsafe.Pointer(p))
	return C.glitch_remove_sample(p) == 0
}

type Glitch interface {
	Compile(expr string) error
	MIDI(msg []byte)
	Fill(buf []float32, frames int, channels int)
	Set(name string, value float32)
	Get(name string) float32
	Destroy()
}

var ErrSyntax = errors.New("glitch syntax error")

type glitch struct {
	sync.Mutex
	g *C.struct_glitch
}

func NewGlitch() Glitch {
	g := C.glitch_create()
	if g == nil {
		return nil
	}
	return &glitch{g: g}
}

func (g *glitch) Destroy() {
	g.Lock()
	defer g.Unlock()
	C.glitch_destroy(g.g)
}

func (g *glitch) Compile(expr string) error {
	g.Lock()
	defer g.Unlock()
	p := C.CString(expr)
	defer C.free(unsafe.Pointer(p))
	r := C.glitch_compile(g.g, p, C.strlen(p))
	if r != 0 {
		return ErrSyntax
	}
	return nil
}

func (g *glitch) Fill(buf []float32, frames, channels int) {
	g.Lock()
	defer g.Unlock()
	C.glitch_fill(g.g, (*C.float)(&buf[0]), C.size_t(frames), C.size_t(channels))
}

func (g *glitch) MIDI(msg []byte) {
	if len(msg) == 3 {
		g.Lock()
		defer g.Unlock()
		C.glitch_midi(g.g, C.uchar(msg[0]), C.uchar(msg[1]), C.uchar(msg[2]))
	}
}

func (g *glitch) Set(name string, value float32) {
	p := C.CString(name)
	defer C.free(unsafe.Pointer(p))
	g.Lock()
	defer g.Unlock()
	C.glitch_set(g.g, p, C.float(value))
}

func (g *glitch) Get(name string) float32 {
	p := C.CString(name)
	defer C.free(unsafe.Pointer(p))
	g.Lock()
	defer g.Unlock()
	return float32(C.glitch_get(g.g, p))
}
