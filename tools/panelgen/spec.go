package main

import (
	"encoding/json"
	"fmt"
	"os"
	"regexp"
)

// Geometry from docs/design-system.md, in Rack px (75 DPI, 1 HP = 15 px).
const (
	pxPerHP       = 15.0
	panelHeightMM = 128.5 // Rack convention; the grid itself is 380 px
	mmPerPx       = 25.4 / 75

	bandY = 17.0
	bandH = 19.0

	plateTop    = 320.0
	plateBottom = 363.0
	plateRadius = 1.5
	plateMargin = 6.0

	outputRowY = 345.5
	rowPitch   = 48.0
	labelGap   = 3.0

	screwSize  = 15.0
	screwRowY  = 365.0
	smallPanel = 6 // HP below which a panel has one screw top and bottom
)

// Typography, sizes in px at 75 DPI.
const (
	titleSize      = 10.5
	titleSizeSmall = 8.5 // 3 and 4 HP
	labelSize      = 6.5
	labelTracking  = 0.04 // em
)

// Colours.
const (
	colGround     = "#1f2123"
	colLabel      = "#d2d6d9"
	colPlate      = "#d9dcdf"
	colPlateLabel = "#1f2123"
	colTitle      = "#ffffff"
	colMark       = "#9aa0a5"
)

var families = map[string]string{
	"trades":  "#b8643f",
	"weather": "#557589",
	"kitchen": "#9c6a12",
}

// Kind is a part placed on the panel. Its diameter, in mm, sets where the label goes.
type Kind string

const (
	KnobLarge Kind = "knob-large"
	Knob      Kind = "knob"
	Trimpot   Kind = "trimpot"
	Input     Kind = "input"
	Output    Kind = "output"
	Light     Kind = "light"
)

var diameters = map[Kind]float64{
	KnobLarge: 12.19,
	Knob:      9.60,
	Trimpot:   6.05,
	Input:     8.03,
	Output:    8.03,
	Light:     3.0,
}

// Module is one panel description, read from panels/<Slug>.json.
type Module struct {
	Slug     string    `json:"slug"`
	Name     string    `json:"name"`
	Family   string    `json:"family"`
	HP       int       `json:"hp"`
	Controls []Control `json:"controls"`
}

// Control is a knob, jack or light. Row 0 is the output row at the bottom;
// each row up is 48 px higher. Controls in a row share its width equally, in
// the order they are listed.
type Control struct {
	ID    string `json:"id"`
	Kind  Kind   `json:"kind"`
	Label string `json:"label"`
	Row   int    `json:"row"`
}

// Placed is a control with its centre in px.
type Placed struct {
	Control
	X, Y float64
}

var (
	slugRe = regexp.MustCompile(`^[A-Za-z0-9_-]+$`)
	idRe   = regexp.MustCompile(`^[A-Z][A-Z0-9_]*$`)
)

func loadModule(path string) (*Module, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	var m Module
	if err := json.Unmarshal(data, &m); err != nil {
		return nil, fmt.Errorf("%s: %w", path, err)
	}
	if err := m.check(); err != nil {
		return nil, fmt.Errorf("%s: %w", path, err)
	}
	return &m, nil
}

func (m *Module) check() error {
	if !slugRe.MatchString(m.Slug) {
		return fmt.Errorf("slug %q must be plain ASCII letters, digits, - or _", m.Slug)
	}
	if m.Name == "" {
		return fmt.Errorf("name is empty")
	}
	if _, ok := families[m.Family]; !ok {
		return fmt.Errorf("unknown family %q", m.Family)
	}
	if m.HP < 3 {
		return fmt.Errorf("hp %d is below 3", m.HP)
	}
	ids := map[string]bool{}
	large := 0
	jacks := map[int]int{}
	for _, c := range m.Controls {
		if !idRe.MatchString(c.ID) {
			return fmt.Errorf("control id %q must be an upper-case C++ identifier", c.ID)
		}
		if ids[c.ID] {
			return fmt.Errorf("control id %q is used twice", c.ID)
		}
		ids[c.ID] = true
		if _, ok := diameters[c.Kind]; !ok {
			return fmt.Errorf("%s: unknown kind %q", c.ID, c.Kind)
		}
		if c.Row < 0 || outputRowY-rowPitch*float64(c.Row) < bandY+bandH+rowPitch/2 {
			return fmt.Errorf("%s: row %d is off the panel", c.ID, c.Row)
		}
		if c.Kind == Output && c.Row != 0 {
			return fmt.Errorf("%s: outputs go in row 0, on the plate", c.ID)
		}
		if c.Kind == Output && m.HP < smallPanel {
			return fmt.Errorf("%s: below %d HP the mark sits where the plate would; not designed yet", c.ID, smallPanel)
		}
		if c.Kind == KnobLarge {
			large++
		}
		if c.Kind == Input || c.Kind == Output {
			jacks[c.Row]++
		}
	}
	if large > 1 {
		return fmt.Errorf("at most one knob-large per module")
	}
	for row, n := range jacks {
		if n > m.HP/2 && n > 1 {
			return fmt.Errorf("row %d holds %d jacks; %d HP fits at most %d", row, n, m.HP, max(m.HP/2, 1))
		}
	}
	return nil
}

func (m *Module) width() float64 { return pxPerHP * float64(m.HP) }

func rowY(row int) float64 { return outputRowY - rowPitch*float64(row) }

// place gives every control its centre: x from its column, y from its row.
func (m *Module) place() []Placed {
	count := map[int]int{}
	for _, c := range m.Controls {
		count[c.Row]++
	}
	seen := map[int]int{}
	w := m.width()
	out := make([]Placed, 0, len(m.Controls))
	for _, c := range m.Controls {
		i := seen[c.Row] + 1
		seen[c.Row] = i
		n := count[c.Row]
		out = append(out, Placed{c, w * float64(2*i-1) / float64(2*n), rowY(c.Row)})
	}
	return out
}

// screws returns the top-left corner of each screw, in px.
func (m *Module) screws() [][2]float64 {
	w := m.width()
	if m.HP < smallPanel {
		x := (w - screwSize) / 2
		return [][2]float64{{x, 0}, {x, screwRowY}}
	}
	return [][2]float64{
		{screwSize, 0}, {w - 2*screwSize, 0},
		{screwSize, screwRowY}, {w - 2*screwSize, screwRowY},
	}
}

func radiusPx(k Kind) float64 { return diameters[k] / mmPerPx / 2 }
