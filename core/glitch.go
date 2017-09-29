package core

/*
#cgo CFLAGS: -std=c99 -Wall -Wextra -pedantic -Wno-unused-parameter
#cgo LDFLAGS: -lm

#include "glitch.h"
*/
import "C"
import (
	"errors"
	"time"
	"unsafe"
)

func init() {
	Init(44100, uint64(time.Now().UnixNano()))
}

func Init(sr int, seed uint64) {
	C.glitch_init(C.int(sr), C.ulonglong(seed))
}

type Glitch interface {
	Compile(expr string) error
	Fill(buf []float32, frames int, channels int)
	Destroy()
}

var ErrSyntax = errors.New("glitch syntax error")

type glitch struct {
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
	C.glitch_destroy(g.g)
}

func (g *glitch) Compile(expr string) error {
	p := C.CString(expr)
	defer C.free(unsafe.Pointer(p))
	r := C.glitch_compile(g.g, p, C.strlen(p))
	if r != 0 {
		return ErrSyntax
	}
	return nil
}

func (g *glitch) Fill(buf []float32, frames, channels int) {
	C.glitch_fill(g.g, (*C.float)(&buf[0]), C.size_t(frames), C.size_t(channels))
}
