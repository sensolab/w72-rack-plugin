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
			// Static Google Fonts builds keep kerning in GPOS, which sfnt
			// doesn't read; a legacy kern table is used when there is one.
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

// PathCentered returns SVG path data, in mm, for the string centred on x with
// its baseline at y (both in px).
func (t Text) PathCentered(x, baseline float64) (string, error) {
	w, err := t.Width()
	if err != nil {
		return "", err
	}
	return t.Path(x-w/2, baseline)
}

// Path returns SVG path data, in mm, for the string starting at x with its
// baseline at y (both in px).
func (t Text) Path(x, baseline float64) (string, error) {
	gs, err := t.glyphs()
	if err != nil {
		return "", err
	}
	adv, err := t.advances(gs)
	if err != nil {
		return "", err
	}
	fc := t.Face
	scale := t.Size / float64(fc.upem) // 26.6 font units to px
	pt := func(p fixed.Point26_6, ox float64) string {
		// sfnt outlines already have y pointing down, like SVG.
		return num((ox+float64(p.X)*scale)*mmPerPx) + " " + num((baseline+float64(p.Y)*scale)*mmPerPx)
	}
	var b strings.Builder
	for i, g := range gs {
		segs, err := fc.f.LoadGlyph(&fc.buf, g, fc.upem, nil)
		if err != nil {
			return "", err
		}
		open := false
		for _, s := range segs {
			switch s.Op {
			case sfnt.SegmentOpMoveTo:
				if open {
					b.WriteString("Z")
				}
				b.WriteString("M" + pt(s.Args[0], x))
				open = true
			case sfnt.SegmentOpLineTo:
				b.WriteString("L" + pt(s.Args[0], x))
			case sfnt.SegmentOpQuadTo:
				b.WriteString("Q" + pt(s.Args[0], x) + " " + pt(s.Args[1], x))
			case sfnt.SegmentOpCubeTo:
				b.WriteString("C" + pt(s.Args[0], x) + " " + pt(s.Args[1], x) + " " + pt(s.Args[2], x))
			}
		}
		if open {
			b.WriteString("Z")
		}
		x += adv[i]
	}
	return b.String(), nil
}
