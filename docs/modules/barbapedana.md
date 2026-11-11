# Barbapedana

*A 16-step sequencer crossed with a random loop: each step is yours by a chance you set, and anchors capture the loop's notes.*

## What it does

Barbapedana is a step sequencer with a random loop playing behind it. Each step plays either the note you set or the loop's note, and its KEEP knob decides how often it's yours: always, never, or by a coin on each pass. MUTATE sets how often the loop changes its notes.

When the loop plays a note you like, press that step's anchor. The knob turns to that note, and the step keeps it. The machine improvises, and you keep the phrases you like.

Each step also has a chance of firing at all, and a gate output of its own. There's no quantizer: set the span with RANGE and OFFSET, and put a quantizer after it. Barbapedana is 30 HP wide and needs a clock in CLOCK.

## The panel

![Barbapedana panel](barbapedana.png)

| Control | What it does |
| --- | --- |
| PAGE | What the 16 step knobs set: VALUE (the note), KEEP (how often the step plays it) or CHANCE (how often it fires). |
| Display | The page you're on, the step playing and the CV. On VALUE, the solid bars are your notes and the outlines the loop's. An anchored step has a dot, and its outline is dim, since it always plays your note. |
| LENGTH | How many steps play, 1 to 16. |
| MUTATE | How often the loop changes. Fully left, it repeats. |
| RANGE, OFFSET | The span of the CV: RANGE wide (2 V, two octaves, by default), starting at OFFSET. |
| CLOCK | Moves on one step. Required. |
| RESET | The next clock plays step 1. |
| MUTATE, CHANCE, KEEP inputs | CV added to MUTATE, and to every step's CHANCE or KEEP at once. |
| TURING | Optional. The loop takes its new notes from this input. |
| Step knob | The step's VALUE, KEEP or CHANCE, whichever PAGE shows. |
| Anchor | Lit, the step always plays its VALUE. Press it on a free step to keep the loop's note; press again to free it. Ctrl+Z undoes either. |
| CV | The note playing. It changes only when a gate fires. |
| GATE | The gate of every step that fires. |
| 1 to 16 | Each step's own gate. |

## Right-click options

The defaults are fine to leave alone.

| Option | What it does | Default |
| --- | --- | --- |
| Gate outputs | Gates half a step long, or short triggers. | Gate |
| CV on silent steps | Hold keeps the last note. Move changes on every step, for modulation. | Hold |
| Loop draws | Walk moves each note a little. Anywhere jumps. Turing bits keeps neighbouring steps related. | Walk |

## Samples

### Start here: catch a phrase

1. Add Barbapedana, Moleta, and VCV's VCO, ADSR, VCA, Quantizer and Audio.
2. Patch Moleta's output **1** into **CLOCK**. Patch Barbapedana's **CV** through the Quantizer into the VCO, and **GATE** into the ADSR. Run the VCO through the VCA, with the ADSR on its CV, into Audio.
3. You're hearing the loop. Turn **MUTATE** fully left and it repeats.
4. When a step plays a note you like, press its anchor.
5. Turn **MUTATE** up to about a third. The free steps wander around the notes you kept.

### More ideas

**Program, then let go.** Set your notes, put every KEEP at 100%, then pull a few steps down to half.

**Fate on a CV.** An LFO into **KEEP**: up, your program plays; down, the loop takes over.

**Rhythm too.** On the CHANCE page, turn the off-beats down.

**Drums from the steps.** Patch a few step outputs into drum voices.

**When nothing plays.** Barbapedana needs a clock. If the steps move but nothing sounds, check the CHANCE page.

<!-- Sample patch to come: built in Rack with VCV Fundamental modules only, saved to patches/. -->

## Where it comes from

<!-- Photo to come: Barbapedana, the Milanese street singer, public domain or CC-licensed, with its credit. -->

*Barbapedana: the Milanese street singer of the 1800s, remembered in the song that carries his name: familiar tunes, verses of his own.*

My melodies come from random loops and a quantizer far more often than from programmed steps. OrangeLine Mother is in 62 of my 182 saved patches, while plain step sequencers show up in a handful. A random loop wanders with nothing to hold on to, and a step sequencer doesn't wander at all.

OrangeLine Morpheus comes closest, but its LOCK is one dial for the whole loop. I wanted steps 1, 4 and 8 always mine and the rest free. Barbapedana puts that chance on every step, and the anchors catch what the loop throws.

Timing details, edge cases and the saved state are in the [reference](barbapedana-reference.md).
