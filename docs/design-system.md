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
| Light | white | `MediumLight<WhiteLight>`, 3 mm |

The family bands:

| Family | Band | White title contrast | Modules |
| --- | --- | --- | --- |
| Trades | terracotta `#b8643f` | about 4.3:1 | Brumista, Ghisa, Magutt |
| Weather and light | fog blue `#557589` | about 4.9:1 | Scighera, Gibigiana, Galaverna, Fiòca, Merla, Giazz |
| Kitchen | saffron `#9c6a12` | about 4.7:1 | Cassoeula, Michetta |

Colour on a panel means family and nothing else. Signal types are carried by cable colours in the patch, so jacks and labels stay grey.

## Type

One typeface, [Inter Tight](https://fonts.google.com/specimen/Inter+Tight) (SIL Open Font License). Its outlines are baked into the SVG at build time; no font ships with the plugin.

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

Rack's stock parts, chosen for a dark panel. Nothing custom yet.

| Part | Class | Size |
| --- | --- | --- |
| Main control (at most one per module) | `RoundLargeBlackKnob` | 12.19 mm |
| Other controls | `RoundBlackKnob` | 9.60 mm |
| CV amounts | `Trimpot` | 6.05 mm |
| Jacks | `DarkPJ301MPort` | 8.03 mm |
| Screws | `ScrewBlack` | 5.08 mm |
| Lights | `MediumLight<WhiteLight>` | 3 mm |

`DarkPJ301MPort` and `ScrewBlack`, not the `Themed*` classes: a themed part turns light when *Use dark panels if available* is off, and these panels are always dark.

## How panels get made

The design is decided on the Claude Design canvas; the SVGs in `res/` are built from it by `tools/panelgen`, a Go program (Go is already in the toolchain for the MCP server). Nobody edits a panel SVG by hand.

1. Each module has a description, `panels/<Slug>.json`: slug, display name, family, HP, and its controls, each with an id, a kind (`knob-large`, `knob`, `trimpot`, `input`, `output`, `light`), a label and a row.
2. **Rows count from the bottom.** Row 0 is the output row on the plate (y = 345.5 px), and each row up is 48 px higher. Controls in a row share its width equally, in the order they are listed, so a description never holds a coordinate.
3. `make panels` writes `res/<Slug>.svg` in mm (ground, band and title, labels, plate, mark) and `src/<Slug>Panel.hpp`, which holds each control's centre and the screws' corners in px. The module widget places its parts from that header, so the widget and the SVG can't disagree.
4. Every label is turned into outlines from the static Inter Tight SemiBold and Bold files in `tools/panelgen/fonts/` (read with `golang.org/x/image/font/sfnt`). They stay out of the plugin package.
5. The generator refuses what nanosvg can't draw (`text`, `style`, `class`, `use`, clips, masks, filters, dashes), and a description that breaks the grid: a second large knob, an output off row 0, more jacks in a row than HP / 2.
6. CI runs `go test` in `tools/panelgen`, which also fails when a committed panel or header differs from what the generator writes now.

Kerning is not applied yet: Inter Tight keeps it in its `GPOS` table, which `sfnt` doesn't read. Labels are short capitals and the difference is small, but a long mixed-case name may want it later.

## Open questions

- [x] Which language for the generator? Go, not Python with fontTools as Sapphire does, so there's no second toolchain.
- [ ] What goes on Brumista's panel besides the link light, once milestone 1 defines its states?
- [ ] Outputs on a panel under 6 HP: the mark sits where the plate would. The generator refuses it until it's designed.
- [ ] Custom knobs to match the panels, or stock parts for good?
