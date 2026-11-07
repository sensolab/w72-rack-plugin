# W72 — Panel Design System

2026-11-01

## The look: Metropolitana

W72 panels are dark, quiet and labelled like a city's signage. The style is called Metropolitana, after Bob Noorda's 1964 signage for the Milan Metro, where a coloured band carries the station's name. On a W72 panel the band carries the module's name, and its colour says which family the module belongs to (see [concept.md](concept.md#naming-grammar)).

- **Dark only.** No light variant; `createPanel` gets one SVG.
- **One band, one plate.** The coloured band at the top and the light output plate at the bottom are the only strong shapes. Everything between is dark ground and small grey labels.
- **Readable at 100% zoom.** Labels are as large as VCV's smallest, so a panel reads without zooming in.
- **Nothing Rack can't draw.** Flat fills and solid hairlines only; type is turned into outlines when a panel is built.
- **The W72 mark signs every panel**, small, at the foot. It's the only element taken from the W72 web design system; the rest of that system doesn't apply to panels.

The design was chosen on a Claude Design canvas from three directions (Nebbia, Lampione, Metropolitana), then tested on four mock modules across the three families.

## What Rack can draw

Rack parses a panel with nanosvg at 75 DPI and draws it with nanovg (`src/window/Svg.cpp`), so a panel is a flat drawing:

| Works | Doesn't |
| --- | --- |
| Paths, rects, circles, ellipses, lines, polygons, groups, transforms | `text` and fonts: labels must be outlines |
| Solid fills and strokes, opacity, line caps and joins | Dashes (drawn solid), gradient strokes |
| Linear and radial gradients, first and last stop only | `use`, `image`, `clipPath`, `mask`, `filter`, `pattern` |
| Inline `style=""` and presentation attributes | `<style>` elements and `class` |
| `display:none`, which hides the components layer | |

Knobs, jacks, screws and lights are separate widgets drawn on top; the panel draws only what surrounds them. Rack adds its own 1 px border and the module shadow.

## Geometry

Rack measures in pixels at 75 DPI, where 1 HP is exactly 15 px, so the grid below is in px with millimetres alongside. Panel SVGs are written in mm.

| Element | px (75 DPI) | mm |
| --- | --- | --- |
| Panel | 15 × HP wide, 380 tall | 5.08 × HP, 128.5 |
| Band | full width, from y = 17, 19 tall | from 5.76, 6.43 tall |
| Output plate | bottom row, y = 320 to 363, corners 1.5, at least 6 in from the panel edge | 108.4 to 122.9, corners 0.5 |
| Output jack row | centres at y = 345.5 | 117.0 |
| Input row above it | centres at y = 297.5 (48 px up, VCV's row pitch) | 100.8 |
| Label to control | about 3 between the label and the top of what it names | about 1 |

**Columns.** A row of *n* controls shares the panel width equally: centres at W × (2i − 1) / 2n. Jack centres in a row stay at least 30 px (2 HP) apart, so a row holds at most HP / 2 jacks; a 3 HP panel holds one.

**Screws.** `ScrewBlack`. From 6 HP up, Rack's four positions (x = 15 and W − 30; y = 0 and 365). Below 6 HP, one screw at the top and one at the bottom, centred at x = (W − 15) / 2.

**The mark.** From 6 HP up: 15 px (5.08 mm) wide, centred between the bottom screws and on their row. Below 6 HP: 22 px (7.45 mm) wide, centred, its bottom edge at y = 361, just above the single screw.

## Colour

| Role | Colour | Note |
| --- | --- | --- |
| Ground | `#1f2123` | The whole panel |
| Labels | `#d2d6d9` | Controls and inputs |
| Output plate | `#d9dcdf` | Outputs look inverted, as VCV's Panel Guide asks |
| Output labels | `#1f2123` | On the plate |
| Band title | `#ffffff` | |
| Mark | `#9aa0a5` strokes | Its two cyans (`#22d3ee`, `#0891a8`) never change |
| Divider | `#34373a` | A 1 px hairline between groups of rows, 12 px in from each side |
| Plate rule | `#b4b9be` | A 1 px rule between groups of outputs, 6 px in from the plate's top and bottom |
| Light | white | `MediumLight<WhiteLight>`, 3 mm |

The family bands:

| Family | Band | White title contrast | Modules |
| --- | --- | --- | --- |
| Trades | terracotta `#b8643f` | about 4.3:1 | Brumista, Ghisa, Moleta, Magutt |
| Weather and light | fog blue `#557589` | about 4.9:1 | Scighera, Gibigiana, Galaverna, Fiòca, Merla, Giazz |
| Kitchen | saffron `#9c6a12` | about 4.7:1 | Cassoeula, Michetta |

Colour on a panel means family and nothing else. Signal types are carried by cable colours in the patch, so jacks and labels stay grey.

## Type

One typeface, [Inter Tight](https://fonts.google.com/specimen/Inter+Tight) (SIL Open Font License). Panel labels are outlines baked into the SVG at build time. Only displays draw text live, so one weight, SemiBold, ships in `res/fonts/` for them (see [Displays](#displays)).

| Use | Weight and case | Size (px at 75 DPI) | Capital height |
| --- | --- | --- | --- |
| Module name, in the band | Bold, mixed case, centred | 10.5 | 2.6 mm |
| Module name on 3 and 4 HP | Bold, mixed case, centred | 8.5 | 2.1 mm |
| Control and input labels | SemiBold, all caps, tracking 0.04 em | 6.5 | 1.6 mm, VCV's smallest |
| Output labels, on the plate | Bold, all caps, tracking 0.04 em | 6.5 | 1.6 mm |

- The band shows the display name, accents included (Fiòca); the slug stays ASCII.
- Labels are one word where possible: `TIME`, `IN L`, `CLOCK`. A CV input takes the name of the control it modulates.
- Labels sit above the knob or jack they name; a light's label sits below it.

## Parts

Rack's stock parts, chosen for a dark panel, and the flat button, the first custom part.

| Part | Class | Size |
| --- | --- | --- |
| Main control (at most one per module) | `RoundLargeBlackKnob` | 12.19 mm |
| Other controls | `RoundBlackKnob` | 9.60 mm |
| CV amounts, secondary controls | `Trimpot` | 6.05 mm |
| Jacks | `DarkPJ301MPort` | 8.03 mm |
| Screws | `ScrewBlack` | 5.08 mm |
| Lights | `MediumLight<WhiteLight>` | 3 mm |
| Button | custom: latching, 14 px bezel, flat square light 9 px | see [Displays](#displays) |

A trimpot is first a CV amount. It also takes a row's secondary controls, such as a level or a pan beside the knobs that define the row, and a strip of settings the rows share, where a full knob for each would not fit the width. What a row is about stays a knob. Gibigiana is the first panel to use them this way.

`DarkPJ301MPort` and `ScrewBlack`, not the `Themed*` classes: a themed part turns light when *Use dark panels if available* is off, and these panels are always dark.

## Displays

Designed before any module needed one, and mocked up on Ghisa and Scighera. The generator builds the wells and positions; `src/widgets.hpp` draws the flat button and the bars (Ghisa uses them) and the readout (Moleta, Moleta + and Gibigiana use it), and the keyboard waits for its first module. The sizes come from the mocks and were tuned by eye.

A display has two halves. The well and its label are panel SVG, made by the generator like everything else. What lights up is a widget drawn live with NanoVG on Rack's light layer, so it stays lit when the room is dimmed and sharp at any zoom.

| Display | Shows | Looks |
| --- | --- | --- |
| Readout | A short value: a note, a time, a step | Signage: a large line over a small one naming it (`C♯3` over `DORIAN`, `2.4 s` over `TIME`) |
| Keyboard | A quantizer's scale | Twelve flat square lights laid out as one octave of keys: dim for a note in the scale, full for the note playing |
| Bars | A row or column of states, such as a register's bits | Flat rectangular lights, lit or off |
| Single light | One state, such as Brumista's LINK | `MediumLight<WhiteLight>`, as in [Parts](#parts) |

- **Signage, not LEDs.** A readout is type, as on Noorda's signs: Inter Tight SemiBold, 14 px for the value and 6.5 px capitals for its name. Seven segments can't spell a note or a scale, and a dot matrix holds one line at this size and comes apart at 100% zoom.
- **White ink**, `#f1f3f4`, like the lights, so colour on a panel still means family. A highlight colour is left open until a module needs one, and is chosen then for that use.
- **One row.** A readout takes one 48 px row: a well 38 px tall, 10 px in from each side, ground `#151718`, a `#2c2f32` hairline, corners 1.5 like the plate. Half rows wait until a module needs them.
- **Dim for a value with no effect.** A readout value that is set but does nothing right now, such as a Gibigiana step that reaches past its buffer, is drawn in the dim ink.
- **Fractions are composed.** Inter Tight has ⅓ and ½ but no ⅔, so a readout writes every fraction from superior digits, the fraction slash and inferior digits (`¹⁄₃`, `²⁄₃`, `1¹⁄₂`), which match its own fractions.
- **Flat lights for arrays, round for single states.** The keyboard's square light is the first W72 part; bars are the same part, stretched. Off is `#2b2d30`; dim is the ink at about 40%. A row of bars may carry a 1 px dim underline in the gap above the well's label, to mark a span of the row (Ghisa's hold loop); it is drawn only while that span means something.
- **A sample in the browser.** The module browser draws a panel with no module behind it, so each display shows a sample value there.
- **Generator kinds.** `bars` (`count`) and `keyboard` are displays: a well 38 px tall and 10 px in from each side, with the label inside it at the foot. `button` is a 14 px bezel with its label below. `button-grid` (`cols`, `rows`) is buttons at a 30 px (2 HP) pitch both ways, centred, with no label. The generator draws the wells and bezels; the header lists the boxes and centres the widget draws its lights in (bars left to right, keyboard C to B, grids row by row). `readout` (`cells`, `rules`) is a well with one cell per column: each cell's name is an outline in the panel, below the value, and the widget draws the value live, 14 px, centred on the column. The header lists each cell's value baseline and a sample value for the module browser. `rules` are hairlines between groups of cells.
- **Off the grid.** A control takes `dy`, a nudge in px of up to half a row, and a display or button may sit in the top row, with its top 6 px under the band. A display or grid takes its whole row.

## How panels get made

The design is decided on the Claude Design canvas; the SVGs in `res/` are built from it by `tools/panelgen`, a Go program (Go is already in the toolchain for the MCP server). Nobody edits a panel SVG by hand.

1. Each module has a description, `panels/<Slug>.json`: slug, display name, family, HP, and its controls, each with an id, a kind (`knob-large`, `knob`, `trimpot`, `input`, `output`, `light`, `button`, `button-grid`, `bars`, `keyboard`, `readout`), a label and a row, and optionally a `dy` nudge.

   A module may set `dividers`, rows to draw a hairline midway below, and `plateRules`, outputs to draw a rule after on the plate. Gibigiana uses both, to set its shared strip apart from the tap rows and its stereo sum from the tap outputs.

   A module may also set `columns` (`inset`, `pitch`, in px), a fixed grid of columns; a control then takes `col` (from 0) instead of sharing its row's width, and a row holds one kind or the other. Moleta uses it: seven columns 37 px apart, 24 px in from the edges, which the equal shares of an 18 HP row can't give. With columns, the output plate runs the panel's width less its margins.
2. **Rows count from the bottom.** Row 0 is the output row on the plate (y = 345.5 px), and each row up is 48 px higher. Controls in a row share its width equally, in the order they are listed, so a description never holds a coordinate.
3. `make panels` writes `res/<Slug>.svg` in mm (ground, band and title, labels, plate, mark) and `src/<Slug>Panel.hpp`, which holds each control's centre and the screws' corners in px. The module widget places its parts from that header, so the widget and the SVG can't disagree.
4. Every label is turned into outlines from the static Inter Tight SemiBold and Bold files in `tools/panelgen/fonts/` (read with `golang.org/x/image/font/sfnt`). They stay out of the plugin package. Rack ignores fill rules and takes a contour for a hole by counting crossings, so Inter Tight's overlapping outlines (the stem and bowl of D, the legs and bar of A) would lose a sliver. The generator keeps each outline with its own counters and puts overlapping outlines in separate paths, and a test replays Rack's hole check on every glyph.
5. The generator refuses what nanosvg can't draw (`text`, `style`, `class`, `use`, clips, masks, filters, dashes), and a description that breaks the grid: a second large knob, an output off row 0, more jacks in a row than HP / 2.
6. CI runs `go test` in `tools/panelgen`, which also fails when a committed panel or header differs from what the generator writes now.

Kerning is not applied yet: Inter Tight keeps it in its `GPOS` table, which `sfnt` doesn't read. Labels are short capitals and the difference is small, but a long mixed-case name may want it later.

## Open questions

- [x] Which language for the generator? Go, not Python with fontTools as Sapphire does, so there's no second toolchain.
- [x] What goes on Brumista's panel besides the link light? A CMD light that flashes per command, dim for reads and full for patch edits, and a slow LINK blink while the port is taken (see the Decisions table in [architecture.md](architecture.md)).
- [ ] Outputs on a panel under 6 HP: the mark sits where the plate would. The generator refuses it until it's designed.
- [ ] Custom knobs to match the panels, or stock parts for good?
- [ ] Small screens that plot values (a register's voltages, an envelope): not designed until a module needs one.
- [ ] The display highlight colour, when a module first needs one.
