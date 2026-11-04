package main

import (
	"fmt"
	"os"
	"strings"

	"golang.org/x/image/font"
	"golang.org/x/image/font/sfnt"
	"golang.org/x/image/math/fixed"
)

// Face is one weight of Inter Tight, read at its own units per em so the
// outlines come out exact and are scaled in float afterwards.
type Face struct {
	f    *sfnt.Font
	buf  sfnt.Buffer
	upem fixed.Int26_6
	cap  float64 // capital height, in em
}

func loadFace(path string) (*Face, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	f, err := sfnt.Parse(data)
	if err != nil {
		return nil, fmt.Errorf("%s: %w", path, err)
	}
	face := &Face{f: f, upem: fixed.I(int(f.UnitsPerEm()))}
	met, err := f.Metrics(&face.buf, face.upem, font.HintingNone)
	if err != nil {
		return nil, err
	}
	face.cap = float64(met.CapHeight) / float64(face.upem)
	return face, nil
}

// Text is a string set in px at 75 DPI, with tracking in em.
type Text struct {
	Face     *Face
	Size     float64
	Tracking float64
	S        string
}

func (t Text) glyphs() ([]sfnt.GlyphIndex, error) {
	var out []sfnt.GlyphIndex
	for _, r := range t.S {
		g, err := t.Face.f.GlyphIndex(&t.Face.buf, r)
		if err != nil {
			return nil, err
		}
		if g == 0 {
			return nil, fmt.Errorf("no glyph for %q in %q", r, t.S)
		}
		out = append(out, g)
	}
	return out, nil
}

// advances returns each glyph's advance in px, kerning and tracking included,
// with no tracking after the last glyph.
func (t Text) advances(gs []sfnt.GlyphIndex) ([]float64, error) {
	fc := t.Face
	scale := t.Size / float64(fc.upem)
	out := make([]float64, len(gs))
	for i, g := range gs {
		a, err := fc.f.GlyphAdvance(&fc.buf, g, fc.upem, font.HintingNone)
		if err != nil {
			return nil, err
		}
		out[i] = float64(a) * scale
		if i < len(gs)-1 {
			// sfnt reads pair kerning from GPOS, where Inter Tight keeps it.
			if k, err := fc.f.Kern(&fc.buf, g, gs[i+1], fc.upem, font.HintingNone); err == nil {
				out[i] += float64(k) * scale
			}
			out[i] += t.Tracking * t.Size
		}
	}
	return out, nil
}

// Width is the string's advance width in px.
func (t Text) Width() (float64, error) {
	gs, err := t.glyphs()
	if err != nil {
		return 0, err
	}
	adv, err := t.advances(gs)
	if err != nil {
		return 0, err
	}
	w := 0.0
	for _, a := range adv {
		w += a
	}
	return w, nil
}

// CapHeight is the height of a capital in px.
func (t Text) CapHeight() float64 { return t.Face.cap * t.Size }

// PathCentered is Path with the string centred on x.
func (t Text) PathCentered(x, baseline float64) ([]string, error) {
	w, err := t.Width()
	if err != nil {
		return nil, err
	}
	return t.Path(x-w/2, baseline)
}

// Path returns the string as SVG path data, in mm, starting at x with its
// baseline at y (both in px). It returns one path per shape: see shapes.
func (t Text) Path(x, baseline float64) ([]string, error) {
	cs, err := t.contours(x, baseline)
	if err != nil {
		return nil, err
	}
	layers, err := shapes(cs)
	if err != nil {
		return nil, err
	}
	out := make([]string, len(layers))
	for i, l := range layers {
		var b strings.Builder
		for _, c := range l {
			b.WriteString(c.d)
		}
		out[i] = b.String()
	}
	return out, nil
}

// contour is one closed outline of a glyph: SVG path data in mm, and its
// points in px (on-curve and control) for the shape tests.
type contour struct {
	d    string
	pts  [][2]float64 // every point, for the bounding box
	ends [][2]float64 // on-curve points: the polygon Rack tests against
}

// area is the contour's signed area in px². Glyph outlines run one way and
// counters the other; with y down, outlines come out positive.
func (c contour) area() float64 {
	a := 0.0
	for i, p := range c.ends {
		q := c.ends[(i+1)%len(c.ends)]
		a += p[0]*q[1] - q[0]*p[1]
	}
	return a / 2
}

func (c contour) bounds() (minX, minY, maxX, maxY float64) {
	minX, minY, maxX, maxY = c.pts[0][0], c.pts[0][1], c.pts[0][0], c.pts[0][1]
	for _, p := range c.pts {
		minX, minY = min(minX, p[0]), min(minY, p[1])
		maxX, maxY = max(maxX, p[0]), max(maxY, p[1])
	}
	return
}

// contains reports whether p is inside the contour's polygon (even-odd).
func (c contour) contains(p [2]float64) bool {
	in := false
	for i, j := 0, len(c.ends)-1; i < len(c.ends); j, i = i, i+1 {
		a, b := c.ends[i], c.ends[j]
		if (a[1] > p[1]) != (b[1] > p[1]) && p[0] < (b[0]-a[0])*(p[1]-a[1])/(b[1]-a[1])+a[0] {
			in = !in
		}
	}
	return in
}

func (t Text) contours(x, baseline float64) ([]contour, error) {
	gs, err := t.glyphs()
	if err != nil {
		return nil, err
	}
	adv, err := t.advances(gs)
	if err != nil {
		return nil, err
	}
	fc := t.Face
	scale := t.Size / float64(fc.upem) // 26.6 font units to px
	var out []contour
	for i, g := range gs {
		segs, err := fc.f.LoadGlyph(&fc.buf, g, fc.upem, nil)
		if err != nil {
			return nil, err
		}
		var cur *contour
		ox := x
		pt := func(p fixed.Point26_6, end bool) string {
			// sfnt outlines already have y pointing down, like SVG.
			q := [2]float64{ox + float64(p.X)*scale, baseline + float64(p.Y)*scale}
			cur.pts = append(cur.pts, q)
			if end {
				cur.ends = append(cur.ends, q)
			}
			return num(q[0]*mmPerPx) + " " + num(q[1]*mmPerPx)
		}
		var b strings.Builder
		flush := func() {
			if cur != nil {
				b.WriteString("Z")
				cur.d = b.String()
				out = append(out, *cur)
				b.Reset()
			}
		}
		for _, s := range segs {
			switch s.Op {
			case sfnt.SegmentOpMoveTo:
				flush()
				cur = &contour{}
				b.WriteString("M" + pt(s.Args[0], true))
			case sfnt.SegmentOpLineTo:
				b.WriteString("L" + pt(s.Args[0], true))
			case sfnt.SegmentOpQuadTo:
				b.WriteString("Q" + pt(s.Args[0], false) + " " + pt(s.Args[1], true))
			case sfnt.SegmentOpCubeTo:
				b.WriteString("C" + pt(s.Args[0], false) + " " + pt(s.Args[1], false) + " " + pt(s.Args[2], true))
			}
		}
		flush()
		x += adv[i]
	}
	return out, nil
}

// shapes sorts contours into the paths a panel draws them in.
//
// Rack ignores fill-rule and winding: it decides whether each contour of a
// path is a hole by counting how often a short line from it crosses the
// path's other contours (svgDraw in Rack's src/window/Svg.cpp). That works
// for outlines and their counters, but Inter Tight builds some letters from
// overlapping outlines (the stem and bowl of D, the legs and bar of A), and
// there Rack takes the second outline for a hole and cuts a sliver out of
// the letter. So each outline goes with the counters inside it, and outlines
// that overlap go into different paths, which Rack paints one over the other.
func shapes(cs []contour) ([][]contour, error) {
	type island struct {
		parts                  []contour
		minX, minY, maxX, maxY float64
	}
	var islands []*island
	var outlines []int // index into islands, per solid contour
	var holes []contour
	for _, c := range cs {
		if c.area() >= 0 {
			is := &island{parts: []contour{c}}
			is.minX, is.minY, is.maxX, is.maxY = c.bounds()
			islands = append(islands, is)
			outlines = append(outlines, len(islands)-1)
		} else {
			holes = append(holes, c)
		}
	}
	for _, h := range holes {
		best := -1
		for _, i := range outlines {
			o := islands[i].parts[0]
			if o.contains(h.ends[0]) && (best < 0 || o.area() < islands[best].parts[0].area()) {
				best = i
			}
		}
		if best < 0 {
			return nil, fmt.Errorf("a counter outside every outline")
		}
		islands[best].parts = append(islands[best].parts, h)
	}
	var layers [][]*island
	for _, is := range islands {
		placed := false
		for li, l := range layers {
			clear := true
			for _, o := range l {
				if is.minX < o.maxX && o.minX < is.maxX && is.minY < o.maxY && o.minY < is.maxY {
					clear = false
					break
				}
			}
			if clear {
				layers[li] = append(l, is)
				placed = true
				break
			}
		}
		if !placed {
			layers = append(layers, []*island{is})
		}
	}
	out := make([][]contour, len(layers))
	for i, l := range layers {
		for _, is := range l {
			out[i] = append(out[i], is.parts...)
		}
	}
	return out, nil
}
