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
	colDivider    = "#34373a" // a hairline between groups of rows, on the ground
	colPlateRule  = "#b4b9be" // a rule between groups of outputs, on the plate
)

// Rules and dividers, in px.
const (
	ruleWidth      = 1.0
	dividerInset   = 12.0 // in from each side of the panel
	plateRuleInset = 6.0  // in from the plate's top and bottom
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

	// Button is a latching square button with a flat square light.
	Button Kind = "button"
	// ButtonGrid is a grid of buttons at 30 px (2 HP) pitch both ways.
	ButtonGrid Kind = "button-grid"
	// Bars is a display: a row of flat bar lights in a well.
	Bars Kind = "bars"
	// Keyboard is a display: twelve flat square lights laid out as one octave.
	Keyboard Kind = "keyboard"
	// Readout is a display: signage text in cells, one per column, in a well.
	Readout Kind = "readout"
)

var diameters = map[Kind]float64{
	KnobLarge: 12.19,
	Knob:      9.60,
	Trimpot:   6.05,
	Input:     8.03,
	Output:    8.03,
	Light:     3.0,
	Button:    buttonSize * mmPerPx,
}

// Displays and the button grid have no diameter: they size from their own fields.
func isDisplay(k Kind) bool { return k == Bars || k == Keyboard || k == Readout }
func knownKind(k Kind) bool {
	_, ok := diameters[k]
	return ok || isDisplay(k) || k == ButtonGrid
}

// Module is one panel description, read from panels/<Slug>.json.
type Module struct {
	Slug   string `json:"slug"`
	Name   string `json:"name"`
	Family string `json:"family"`
	HP     int    `json:"hp"`
	// Columns, when set, gives controls a named column (Control.Col) instead of
	// an equal share of the width.
	Columns  *Columns  `json:"columns,omitempty"`
	Controls []Control `json:"controls"`
	// Dividers are hairlines across the panel, each midway below the row it names.
	Dividers []int `json:"dividers,omitempty"`
	// PlateRules are rules on the plate, each after the output it names.
	PlateRules []string `json:"plateRules,omitempty"`
}

// Columns is a fixed grid of columns: the first centre Inset px from the left
// edge, then one every Pitch px.
type Columns struct {
	Inset float64 `json:"inset"`
	Pitch float64 `json:"pitch"`
}

// Cell is one value of a readout, over the column it sits in. Label is its
// small name, drawn in the panel; Sample is what the module browser shows.
type Cell struct {
	Col    int    `json:"col"`
	Label  string `json:"label"`
	Sample string `json:"sample"`
}

// Control is a knob, jack or light. Row 0 is the output row at the bottom;
// each row up is 48 px higher. Controls in a row share its width equally, in
// the order they are listed.
type Control struct {
	ID    string `json:"id"`
	Kind  Kind   `json:"kind"`
	Label string `json:"label"`
	Row   int    `json:"row"`
	// Dy nudges the control off its row, in px (down is positive), at most half
	// a row either way. Controls in a row are nudged together by giving them the
	// same Dy. Outputs stay on the plate and take none.
	Dy float64 `json:"dy,omitempty"`
	// Count is the number of bars in a bars display.
	Count int `json:"count,omitempty"`
	// Cols and Rows size a button grid.
	Cols int `json:"cols,omitempty"`
	Rows int `json:"rows,omitempty"`
	// Col places the control in a column of Module.Columns (counted from 0)
	// instead of sharing its row's width. A row holds either kind, not both.
	Col *int `json:"col,omitempty"`
	// Cells and Rules belong to a readout. A rule is a hairline between column
	// n and column n+1.
	Cells []Cell `json:"cells,omitempty"`
	Rules []int  `json:"rules,omitempty"`
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
	if m.Columns != nil && (m.Columns.Pitch <= 0 || m.Columns.Inset < 0) {
		return fmt.Errorf("columns need a positive pitch and an inset of 0 or more")
	}
	ids := map[string]bool{}
	large := 0
	rowCol := map[int]int{}
	jacks := map[int]int{}
	rowCount := map[int]int{}
	rowHasWide := map[int]bool{}
	for _, c := range m.Controls {
		if !idRe.MatchString(c.ID) {
			return fmt.Errorf("control id %q must be an upper-case C++ identifier", c.ID)
		}
		if ids[c.ID] {
			return fmt.Errorf("control id %q is used twice", c.ID)
		}
		ids[c.ID] = true
		if !knownKind(c.Kind) {
			return fmt.Errorf("%s: unknown kind %q", c.ID, c.Kind)
		}
		if err := m.checkPart(c); err != nil {
			return err
		}
		if err := m.checkColumns(c); err != nil {
			return err
		}
		if c.Col != nil {
			rowCol[c.Row]++
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
		rowCount[c.Row]++
		if isDisplay(c.Kind) || c.Kind == ButtonGrid {
			rowHasWide[c.Row] = true
		}
	}
	if large > 1 {
		return fmt.Errorf("at most one knob-large per module")
	}
	for row, n := range rowCol {
		if n != rowCount[row] {
			return fmt.Errorf("row %d mixes controls in columns with controls sharing the width", row)
		}
	}
	for row, n := range rowCount {
		if n > 1 && rowHasWide[row] {
			return fmt.Errorf("row %d holds a display or button grid, which takes the whole row", row)
		}
	}
	for row, n := range jacks {
		if n > m.HP/2 && n > 1 {
			return fmt.Errorf("row %d holds %d jacks; %d HP fits at most %d", row, n, m.HP, max(m.HP/2, 1))
		}
	}
	for _, row := range m.Dividers {
		// Below row 0 is the plate; a divider under the top row would cut the band.
		if row < 1 || rowY(row)-rowPitch/2 < bandY+bandH {
			return fmt.Errorf("divider below row %d is off the panel", row)
		}
	}
	for _, id := range m.PlateRules {
		if i := m.outputIndex(id); i < 0 || i == m.outputCount()-1 {
			return fmt.Errorf("plate rule after %q: not an output with another after it", id)
		}
	}
	return nil
}

// outputIndex is an output's place in row 0, counting from 0, or -1.
func (m *Module) outputIndex(id string) int {
	n := 0
	for _, c := range m.Controls {
		if c.Kind != Output {
			continue
		}
		if c.ID == id {
			return n
		}
		n++
	}
	return -1
}

func (m *Module) outputCount() int {
	n := 0
	for _, c := range m.Controls {
		if c.Kind == Output {
			n++
		}
	}
	return n
}

func (m *Module) width() float64 { return pxPerHP * float64(m.HP) }

// colX is the centre of a column of Module.Columns, in px.
func (m *Module) colX(col int) float64 { return m.Columns.Inset + m.Columns.Pitch*float64(col) }

func rowY(row int) float64 { return outputRowY - rowPitch*float64(row) }

// place gives every control its centre: x from its column, y from its row.
func (m *Module) place() []Placed {
	count := map[int]int{}
	for _, c := range m.Controls {
		if c.Col == nil {
			count[c.Row]++
		}
	}
	seen := map[int]int{}
	w := m.width()
	out := make([]Placed, 0, len(m.Controls))
	for _, c := range m.Controls {
		if c.Col != nil {
			out = append(out, Placed{c, m.colX(*c.Col), rowY(c.Row) + c.Dy})
			continue
		}
		i := seen[c.Row] + 1
		seen[c.Row] = i
		n := count[c.Row]
		out = append(out, Placed{c, w * float64(2*i-1) / float64(2*n), rowY(c.Row) + c.Dy})
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
