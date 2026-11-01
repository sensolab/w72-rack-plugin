package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func testFaces(t *testing.T) Faces {
	t.Helper()
	f, err := loadFaces("fonts")
	if err != nil {
		t.Fatal(err)
	}
	return f
}

func TestColumns(t *testing.T) {
	m := &Module{Slug: "T", Name: "T", Family: "trades", HP: 8, Controls: []Control{
		{ID: "A", Kind: Output, Row: 0},
		{ID: "B", Kind: Output, Row: 0},
		{ID: "C", Kind: Knob, Row: 2},
	}}
	if err := m.check(); err != nil {
		t.Fatal(err)
	}
	p := m.place()
	want := [][2]float64{{30, 345.5}, {90, 345.5}, {60, 249.5}}
	for i, w := range want {
		if p[i].X != w[0] || p[i].Y != w[1] {
			t.Errorf("%s at (%v, %v), want %v", p[i].ID, p[i].X, p[i].Y, w)
		}
	}
	if span, ok := plateSpan(m, p); !ok || span != [2]float64{6, 114} {
		t.Errorf("plate %v, want [6 114]", span)
	}
}

func TestScrews(t *testing.T) {
	small := &Module{HP: 3}
	if got := small.screws(); len(got) != 2 || got[0] != [2]float64{15, 0} || got[1] != [2]float64{15, 365} {
		t.Errorf("3 HP screws %v", got)
	}
	if got := (&Module{HP: 10}).screws(); len(got) != 4 || got[1] != [2]float64{120, 0} {
		t.Errorf("10 HP screws %v", got)
	}
}

func TestCheckRejects(t *testing.T) {
	bad := map[string]*Module{
		"accented slug": {Slug: "Fiòca", Name: "Fiòca", Family: "weather", HP: 4},
		"family":        {Slug: "X", Name: "X", Family: "brass", HP: 4},
		"output row":    {Slug: "X", Name: "X", Family: "trades", HP: 8, Controls: []Control{{ID: "O", Kind: Output, Row: 1}}},
		"too many jacks": {Slug: "X", Name: "X", Family: "trades", HP: 4, Controls: []Control{
			{ID: "A", Kind: Input, Row: 1}, {ID: "B", Kind: Input, Row: 1}, {ID: "C", Kind: Input, Row: 1}}},
		"off the panel": {Slug: "X", Name: "X", Family: "trades", HP: 4, Controls: []Control{{ID: "A", Kind: Knob, Row: 7}}},
	}
	for name, m := range bad {
		if m.check() == nil {
			t.Errorf("%s: accepted", name)
		}
	}
}

func TestTextMetrics(t *testing.T) {
	f := testFaces(t)
	label := Text{f.SemiBold, labelSize, labelTracking, "LINK"}
	// Capitals of a 6.5 px label are VCV's smallest, about 1.6 mm.
	if c := label.CapHeight() * mmPerPx; c < 1.55 || c > 1.65 {
		t.Errorf("label capitals %.3f mm", c)
	}
	plain := label
	plain.Tracking = 0
	w1, _ := label.Width()
	w0, _ := plain.Width()
	if d := w1 - w0; d < 3*0.04*labelSize-1e-9 || d > 3*0.04*labelSize+1e-9 {
		t.Errorf("tracking added %.4f px over 3 gaps", d)
	}
	if _, err := (Text{f.Bold, titleSize, 0, "Fiòca"}).Width(); err != nil {
		t.Errorf("accented name: %v", err)
	}
}

func TestValidate(t *testing.T) {
	for _, bad := range []string{
		`<svg><text>x</text></svg>`,
		`<svg><style>rect{}</style></svg>`,
		`<svg><rect class="a"/></svg>`,
		`<svg><use href="#a"/></svg>`,
		`<svg><path stroke-dasharray="1 1"/></svg>`,
	} {
		if validate(bad) == nil {
			t.Errorf("accepted %s", bad)
		}
	}
	if err := validate(`<svg><g><rect fill="#fff"/></g></svg>`); err != nil {
		t.Error(err)
	}
}

// The committed panels must be what the generator writes now.
func TestGeneratedFilesCurrent(t *testing.T) {
	if err := run(filepath.Join("..", ".."), true); err != nil {
		t.Fatal(err)
	}
}

func TestBrumistaPanel(t *testing.T) {
	m, err := loadModule(filepath.Join("..", "..", "panels", "Brumista.json"))
	if err != nil {
		t.Fatal(err)
	}
	svg, err := panelSVG(m, testFaces(t))
	if err != nil {
		t.Fatal(err)
	}
	for _, want := range []string{`width="15.24mm"`, `id="band"`, `fill="#b8643f"`, `id="label-LINK_LIGHT"`, `id="mark"`} {
		if !strings.Contains(svg, want) {
			t.Errorf("missing %s", want)
		}
	}
	if strings.Contains(svg, `id="plate"`) {
		t.Error("Brumista has no outputs, so no plate")
	}
	if _, err := os.Stat(filepath.Join("fonts", "OFL.txt")); err != nil {
		t.Error("the font licence must ship with the fonts")
	}
}
