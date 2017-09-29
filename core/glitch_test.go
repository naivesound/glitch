package core

import "testing"

func TestGlitch(t *testing.T) {
	g := NewGlitch()
	defer g.Destroy()

	if err := g.Compile("2+"); err != ErrSyntax {
		t.Fatal("expected ErrSyntax", err)
	}

	if err := g.Compile("2+3"); err != nil {
		t.Fatal(err)
	}

	f32 := []float32{0}
	g.Fill(f32, 1, 1)
	if f32[0] != 5 {
		t.Error("expected 5, got", f32)
	}
}
