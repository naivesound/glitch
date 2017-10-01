package core

import "testing"

func eval(g Glitch) float32 {
	f32 := []float32{0}
	g.Fill(f32, 1, 1)
	return f32[0]
}

func TestGlitch(t *testing.T) {
	g := NewGlitch()
	defer g.Destroy()

	if err := g.Compile("2+"); err != ErrSyntax {
		t.Fatal("expected ErrSyntax", err)
	}

	if err := g.Compile("2+3"); err != nil {
		t.Fatal(err)
	}

	if x := eval(g); x != 5 {
		t.Error("expected 5, got", x)
	}
}

func TestGlitchVar(t *testing.T) {
	g := NewGlitch()
	defer g.Destroy()

	g.Compile("x = 5")
	if x := eval(g); x != 5 {
		t.Error("expected 5, got", x)
	}
	if x := g.Get("x"); x != 5 {
		t.Error("expected x=5, got", x)
	}
	g.Compile("z = x + y")
	g.Set("y", 3)
	eval(g)
	if z := g.Get("z"); z != 8 {
		t.Error("expected z=8, got", z)
	}
}
