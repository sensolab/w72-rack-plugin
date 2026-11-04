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

func TestNudgeAndDisplayRow(t *testing.T) {
	m := &Module{Slug: "T", Name: "T", Family: "trades", HP: 8, Controls: []Control{
		{ID: "REG", Kind: Bars, Label: "REG", Row: 6, Dy: 4.5, Count: 16},
		{ID: "K", Kind: Knob, Row: 3, Dy: 8.5},
	}}
	if err := m.check(); err != nil {
		t.Fatal(err)
	}
	p := m.place()
	if p[0].Y != 62 || p[1].Y != 210 {
		t.Errorf("y = %v, %v; want 62, 210", p[0].Y, p[1].Y)
	}
	if w := m.well(p[0]); w != (Box{10, 43, 100, 38}) {
		t.Errorf("well %v", w)
	}
	l := m.lights(p[0])
	if len(l) != 16 || l[0] != (Box{12.5, 48.5, 5, 20}) || l[15].X+l[15].W != 107.5 {
		t.Errorf("bars %v ... %v", l[0], l[15])
	}
	// A display one row up from where Ghisa has it would touch the band.
	m.Controls[0].Row, m.Controls[0].Dy = 7, 0
	if m.check() == nil {
		t.Error("display under the band's floor accepted")
	}
}

func TestButtonGrid(t *testing.T) {
	m := &Module{Slug: "T", Name: "T", Family: "trades", HP: 8, Controls: []Control{
		{ID: "G", Kind: ButtonGrid, Row: 5, Dy: 4.5, Cols: 4, Rows: 2},
	}}
	if err := m.check(); err != nil {
		t.Fatal(err)
	}
	pts := m.gridPoints(m.place()[0])
	if len(pts) != 8 || pts[0] != (Pt{15, 110}) || pts[3] != (Pt{105, 110}) || pts[4] != (Pt{15, 140}) {
		t.Errorf("grid %v", pts)
	}
	m.Controls[0].Cols = 5
	if m.check() == nil {
		t.Error("5 columns at 2 HP pitch accepted on 8 HP")
	}
}

func TestKeyboard(t *testing.T) {
	m := &Module{Slug: "T", Name: "T", Family: "trades", HP: 8, Controls: []Control{
		{ID: "KEYS", Kind: Keyboard, Label: "SCALE", Row: 6, Dy: 4.5},
	}}
	if err := m.check(); err != nil {
		t.Fatal(err)
	}
	l := m.lights(m.place()[0])
	if len(l) != 12 {
		t.Fatalf("%d lights", len(l))
	}
	// Black keys (1, 3, 6, 8, 10) sit in the upper row, between their white neighbours.
	for i, black := range []bool{false, true, false, true, false, false, true, false, true, false, true, false} {
		if black != (l[i].Y == l[0].Y-keyGap-l[0].H) {
			t.Errorf("note %d in the wrong row", i)
		}
	}
	if mid := l[1].X + l[1].W/2; mid <= l[0].X+l[0].W/2 || mid >= l[2].X+l[2].W/2 {
		t.Errorf("C# not between C and D")
	}
	if (&Module{Slug: "T", Name: "T", Family: "trades", HP: 4, Controls: m.Controls}).check() == nil {
		t.Error("keyboard on 4 HP accepted")
	}
}

func TestGhisaPanel(t *testing.T) {
	m, err := loadModule(filepath.Join("..", "..", "panels", "Ghisa.json"))
	if err != nil {
		t.Fatal(err)
	}
	svg, err := panelSVG(m, testFaces(t))
	if err != nil {
		t.Fatal(err)
	}
	for _, want := range []string{`id="well-REGISTER"`, `id="well-SHAPE-7"`, `id="well-HOLD"`, `id="label-REGISTER"`, `id="plate"`} {
		if !strings.Contains(svg, want) {
			t.Errorf("missing %s", want)
		}
	}
	if strings.Contains(svg, `id="label-SHAPE"`) {
		t.Error("the button grid has no label")
	}
}

// rackHole is Rack's own test for whether a contour of a path is a hole
// (svgDraw in src/window/Svg.cpp): count how often a line from the
// contour's first point to just past its top-left corner crosses the
// path's other contours, on-curve point to on-curve point.
func rackHole(c contour, shape []contour, self int) bool {
	cross := func(p0, p1, p2, p3 [2]float64) float64 {
		b := [2]float64{p2[0] - p0[0], p2[1] - p0[1]}
		d := [2]float64{p1[0] - p0[0], p1[1] - p0[1]}
		e := [2]float64{p3[0] - p2[0], p3[1] - p2[1]}
		m := d[0]*e[1] - d[1]*e[0]
		if m == 0 {
			return -1
		}
		return -(d[0]*b[1] - d[1]*b[0]) / m
	}
	minX, minY, _, _ := c.bounds()
	p0, p1 := c.ends[0], [2]float64{minX - 1, minY - 1}
	n := 0
	for i, o := range shape {
		if i == self {
			continue
		}
		for j := range o.ends {
			p2, p3 := o.ends[j], o.ends[(j+1)%len(o.ends)]
			if a, b := cross(p0, p1, p2, p3), cross(p2, p3, p0, p1); 0 <= a && a < 1 && 0 <= b {
				n++
			}
		}
	}
	return n%2 == 1
}

// TestRackSeesHoles checks that Rack takes exactly the counters for holes,
// in every glyph a panel can set, at every size the panels use.
func TestRackSeesHoles(t *testing.T) {
	f := testFaces(t)
	set := "ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789 abcdefghijklmnopqrstuvwxyz àèéìòù -+/&%#"
	for _, face := range []*Face{f.SemiBold, f.Bold} {
		for _, size := range []float64{labelSize, titleSizeSmall, titleSize} {
			for _, s := range strings.Fields(set) {
				cs, err := Text{face, size, labelTracking, s}.contours(0, 20)
				if err != nil {
					t.Fatal(err)
				}
				layers, err := shapes(cs)
				if err != nil {
					t.Fatal(err)
				}
				for _, l := range layers {
					for i, c := range l {
						if got, want := rackHole(c, l, i), c.area() < 0; got != want {
							t.Errorf("%q at %g px: Rack takes a contour at (%.1f, %.1f) for a hole=%v", s, size, c.ends[0][0], c.ends[0][1], got)
						}
					}
				}
			}
		}
	}
}
