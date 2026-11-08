package main

import "fmt"

// The UI parts beyond Rack's stock ones, from docs/design-system.md (Displays)
// and docs/modules/ghisa.md. The panel SVG holds each part's well; the lit
// content is drawn live by the widget, so the generator hands the widget the
// boxes and points to draw it in.
const (
	buttonSize    = 14.0 // bezel of a button, px
	flatLightSize = 9.0  // the flat square light inside a button, px
	gridPitch     = 30.0 // button grid pitch, both ways: 2 HP

	wellHeight = 38.0 // a display takes one 48 px row
	wellMargin = 10.0 // in from each side of the panel
	wellInset  = 2.0  // lights in from the well's edge, sides
	wellTopPad = 5.5  // lights down from the well's top
	wellGap    = 6.0  // least gap between the band and a well
	wellRadius = 1.5  // like the plate

	wellLabelPad = 3.5 // from a display label's baseline to the well's bottom edge

	barHeight = 20.0
	barGap    = 1.0 // between bars; the bar is the pitch less this

	keyGap     = 3.0 // between keyboard lights
	keyMaxSize = 9.0 // keyboard lights, never larger than a button's flat light
	keyMinSize = 4.0

	colWell     = "#151718"
	colWellEdge = "#2c2f32"
)

// Box is a rectangle in px, top-left origin.
type Box struct{ X, Y, W, H float64 }

// Pt is a point in px.
type Pt struct{ X, Y float64 }

// checkPart validates what is particular to a part's kind and nudge.
func (m *Module) checkPart(c Control) error {
	if c.Row < 0 {
		return fmt.Errorf("%s: row %d is off the panel", c.ID, c.Row)
	}
	if c.Dy < -rowPitch/2 || c.Dy > rowPitch/2 {
		return fmt.Errorf("%s: dy %v is more than half a row", c.ID, c.Dy)
	}
	if c.Kind == Output && c.Dy != 0 {
		return fmt.Errorf("%s: outputs sit on the plate and take no dy", c.ID)
	}
	y := rowY(c.Row) + c.Dy
	// Where the part's top edge may reach: a display or a button grid sits right
	// under the band, anything else keeps a half row clear of it.
	floor := bandY + bandH + rowPitch/2
	top := y - radiusPx(c.Kind)
	switch {
	case isDisplay(c.Kind):
		floor, top = bandY+bandH+wellGap, y-wellHeight/2
	case c.Kind == ButtonGrid:
		floor, top = bandY+bandH+wellGap, y-buttonSize/2
	case c.Kind == Button:
		floor, top = bandY+bandH+wellGap, y-buttonSize/2
	default:
		top = y
	}
	if top < floor {
		return fmt.Errorf("%s: row %d is off the panel", c.ID, c.Row)
	}
	switch c.Kind {
	case Bars:
		if c.Count < 2 {
			return fmt.Errorf("%s: bars needs a count of at least 2", c.ID)
		}
		if pitch := m.barPitch(c); pitch-barGap < 2 {
			return fmt.Errorf("%s: %d bars are too many for %d HP", c.ID, c.Count, m.HP)
		}
	case Keyboard:
		if m.keySize() < keyMinSize {
			return fmt.Errorf("%s: a keyboard needs at least %d HP", c.ID, 5)
		}
	case ButtonGrid:
		if c.Cols < 1 || c.Rows < 1 {
			return fmt.Errorf("%s: button-grid needs cols and rows", c.ID)
		}
		if c.Cols > m.HP/2 {
			return fmt.Errorf("%s: %d columns at 2 HP pitch do not fit %d HP", c.ID, c.Cols, m.HP)
		}
		if c.Label != "" {
			return fmt.Errorf("%s: a button grid takes no label; label what it controls elsewhere", c.ID)
		}
	}
	if c.Kind != Bars && c.Count != 0 || c.Kind != ButtonGrid && (c.Cols != 0 || c.Rows != 0) {
		return fmt.Errorf("%s: count, cols and rows belong to bars and button-grid", c.ID)
	}
	return nil
}

func (m *Module) wellWidth() float64 { return m.width() - 2*wellMargin }

func (m *Module) barPitch(c Control) float64 {
	return (m.wellWidth() - 2*wellInset) / float64(c.Count)
}

// keyPitch is the width of a white key; a keyboard fits seven in a well.
func (m *Module) keyPitch() float64 { return (m.wellWidth() - 2*wellInset) / 7 }

func (m *Module) keySize() float64 { return min(m.keyPitch()-keyGap, keyMaxSize) }

// checkColumns validates a control's column, and a readout's cells and rules.
func (m *Module) checkColumns(c Control) error {
	inPanel := func(col int) error {
		if m.Columns == nil {
			return fmt.Errorf("%s: columns are not set for this module", c.ID)
		}
		if x := m.colX(col); col < 0 || x < wellMargin || x > m.width()-wellMargin {
			return fmt.Errorf("%s: column %d is off the panel", c.ID, col)
		}
		return nil
	}
	if c.Col != nil {
		if err := inPanel(*c.Col); err != nil {
			return err
		}
		if c.Kind == ButtonGrid || isDisplay(c.Kind) {
			return fmt.Errorf("%s: a display or button grid takes its whole row, not a column", c.ID)
		}
	}
	if c.Kind != Readout {
		if len(c.Cells) != 0 || len(c.Rules) != 0 {
			return fmt.Errorf("%s: cells and rules belong to a readout", c.ID)
		}
		return nil
	}
	if c.Label != "" {
		return fmt.Errorf("%s: a readout names its cells, not itself", c.ID)
	}
	if len(c.Cells) == 0 {
		return fmt.Errorf("%s: a readout needs cells", c.ID)
	}
	last := -1
	for _, cell := range c.Cells {
		if err := inPanel(cell.Col); err != nil {
			return err
		}
		if cell.Col <= last {
			return fmt.Errorf("%s: cells go left to right, one per column", c.ID)
		}
		last = cell.Col
		if cell.Sample == "" {
			return fmt.Errorf("%s: cell %d has no sample value", c.ID, cell.Col)
		}
	}
	for _, r := range c.Rules {
		if r < c.Cells[0].Col || r >= last {
			return fmt.Errorf("%s: rule after column %d is outside the cells", c.ID, r)
		}
	}
	return nil
}

// Readout cell layout, from the mock: the value's baseline and its name's
// baseline, down from the well's top, and the rule's extent.
const (
	cellValueBase = 21.0
	cellNameBase  = 32.0
	cellRuleTop   = 6.0
	cellRuleEnd   = 32.0
	cellValueSize = 14.0
)

// well is the box a display draws in, and a button its bezel.
func (m *Module) well(p Placed) Box {
	switch {
	case isDisplay(p.Kind):
		s := m.sections()[m.sectionOf(p.Control)]
		return Box{s[0] + wellMargin, p.Y - wellHeight/2, s[1] - s[0] - 2*wellMargin, wellHeight}
	case p.Kind == Button:
		return Box{p.X - buttonSize/2, p.Y - buttonSize/2, buttonSize, buttonSize}
	}
	return Box{}
}

// bezels are the wells drawn in the SVG: a display's well, a button's bezel,
// every button of a grid.
func (m *Module) bezels(p Placed) []Box {
	switch {
	case isDisplay(p.Kind), p.Kind == Button:
		return []Box{m.well(p)}
	case p.Kind == ButtonGrid:
		var out []Box
		for _, pt := range m.gridPoints(p) {
			out = append(out, Box{pt.X - buttonSize/2, pt.Y - buttonSize/2, buttonSize, buttonSize})
		}
		return out
	}
	return nil
}

// gridPoints are the centres of a grid's buttons, row by row, centred on the panel.
func (m *Module) gridPoints(p Placed) []Pt {
	x0 := m.width()/2 - float64(p.Cols-1)*gridPitch/2
	var out []Pt
	for r := 0; r < p.Rows; r++ {
		for c := 0; c < p.Cols; c++ {
			out = append(out, Pt{x0 + float64(c)*gridPitch, p.Y + float64(r)*gridPitch})
		}
	}
	return out
}

// lights are the lit boxes of a display: the bars left to right, or the
// keyboard's twelve notes from C to B.
func (m *Module) lights(p Placed) []Box {
	w := m.well(p)
	switch p.Kind {
	case Bars:
		pitch := m.barPitch(p.Control)
		out := make([]Box, p.Count)
		for i := range out {
			out[i] = Box{w.X + wellInset + float64(i)*pitch + barGap/2, w.Y + wellTopPad, pitch - barGap, barHeight}
		}
		return out
	case Keyboard:
		pitch, s := m.keyPitch(), m.keySize()
		left := w.X + wellInset
		white := func(i int) Box { return Box{left + (float64(i)+0.5)*pitch - s/2, w.Y + wellTopPad + s + keyGap, s, s} }
		black := func(boundary int) Box { return Box{left + float64(boundary)*pitch - s/2, w.Y + wellTopPad, s, s} }
		return []Box{white(0), black(1), white(1), black(2), white(2), white(3), black(4), white(4), black(5), white(5), black(6), white(6)}
	}
	return nil
}
