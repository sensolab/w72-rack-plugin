# Barbapedana

*The Milanese street singer of the 1800s, remembered in the song that carries his name: familiar tunes, verses of his own.*

> **Designed, not built.** This page is the design. It becomes the manual when Barbapedana ships, and gets its `manualUrl` then.

Barbapedana is a 16-step sequencer crossed with a Turing-style random loop. Every step has a value you program and a KEEP chance: at 100% the step always plays your value, at 0% it always plays the random loop, and in between a coin decides on each pass. An anchor button on each step makes it yours, and pressed while the loop is playing it captures the loop's note into the step's knob. The machine improvises, and you keep the phrases you like.

- **Family:** trades · **Width:** about 30 HP · **Slug:** `Barbapedana` · **Since:** not released

## Why

Melodies in the saved patches come from random loops and a quantizer far more often than from programmed steps: OrangeLine Mother is in 62 of 182 patches, SickoCV RandLoops8, Vult Dopamine and OrangeLine Morph follow, while plain step sequencers appear in a handful (Fundamental SEQ3 in 5, Impromptu Gate-Seq-64 in 6). The random loops wander with nothing to hold on to, and the step sequencers don't wander at all.

OrangeLine Morph and Morpheus come closest: they morph between a stored sequence and a random one, with a LOCK chance. But LOCK is one dial for the whole loop. There is no way to say that steps 1, 4 and 8 are always the ones you programmed while the rest are free. Barbapedana puts that chance on every step.

## Prior art

| Module | What it does | How Barbapedana differs |
| --- | --- | --- |
| OrangeLine Morph / Morpheus | Morphs between a given or stored sequence and a random one, one LOCK chance for the loop | KEEP per step, anchors, and capture into the knobs |
| Bruer SEQ1 | Probabilistic Turing sequencer with a quantizer and Euclidean triggers | Programmed steps beside the loop; no quantizer |
| Animatek UZZ, Count Modula Multi-step | 16 steps with a probability per step | The probability chooses between your value and the loop, besides firing the gate |
| OhmerPrems FranKe | Multi-track step sequencer with Turing machines inside | One line where each step is part yours, part the loop's |
| InfiniteNoise Turing Machine | Turing machine with 16 gate outputs | Gate outputs per step, on a sequence you can program |
| SickoCV RandLoops8, Grayscale Permutation | Turing-style looping random voltages | A program to hold the loop against |

## How it works

- **Steps.** 16 steps, each with VALUE, KEEP and CHANCE. LENGTH sets how many steps play (1 to 16). CLOCK advances, RESET returns to step 1.
- **The random loop.** Behind the steps runs a Turing-style loop of 16 values. On each pass, MUTATE is the chance that a free step's loop value is redrawn, so fully left the loop repeats and further right it keeps changing. TURING IN, when patched, replaces the internal randomness: a redrawn value is sampled from the input instead.
- **KEEP.** On each step, a coin with the step's KEEP chance decides whether it plays VALUE or the loop's value. KEEP CV shifts every step's KEEP at once.
- **Anchors.** An anchored step always plays VALUE, whatever KEEP or KEEP CV say. Pressing the anchor on a free step captures the value the loop is playing into VALUE, so the knob turns to it, and locks the step; pressing it again frees the step. A capture is one Ctrl+Z.
- **CHANCE.** The chance that the step fires its gate, on its own output and on GATE. CHANCE CV shifts every step's CHANCE at once, so KEEP CV and CHANCE CV are the two global "how much fate" controls.
- **Pages.** The 16 knobs edit one thing at a time: the PAGE switch picks VALUE, KEEP or CHANCE. Each page has its own 16 parameters and PAGE only shows one set, so the knobs show the stored values of the page you're on, and undo, Randomize, presets, MIDI mappings and Brumista all see stable parameters (KEEP 5 is always KEEP 5).
- **No quantizer.** RANGE and OFFSET scale the CV output, so it needs no rescaler; a quantizer goes downstream.

## Inputs and outputs

| Jack | Does |
| --- | --- |
| CLOCK, RESET | Advance one step; return to step 1 |
| MUT CV | Adds to MUTATE |
| CHANCE CV | Shifts every step's CHANCE |
| TURING IN | Source for the loop's redrawn values, normalled to the internal random |
| KEEP CV | Shifts every free step's KEEP |
| CV | The current step's value, scaled by RANGE and OFFSET |
| GATE | The current step's gate, when its CHANCE coin passes |
| 1 to 16 | Each step's own gate |

## Right-click menu

| Item | Choices | Default |
| --- | --- | --- |
| Gate outputs | Trigger or gate | To be decided |

## Panel

![Barbapedana panel sketch](barbapedana-sketch.png)

A hand-drawn sketch, not a mock: the real panel comes from the generator once the layout is settled on the grid.

- **Left column:** PAGE (a horizontal three-way switch: VALUE, KEEP, CHANCE), LENGTH and MUTATE, RANGE and OFFSET, CLOCK and RESET, MUT CV and CHANCE CV, TURING IN and KEEP CV, then CV and GATE on the plate.
- **A wide display** over the steps: the page, the step and LENGTH (`step 6 / 16`) and the CV going out on its top line; under it all 16 steps as three thin bars each (VALUE, KEEP, CHANCE), a dot on anchored steps and a line under the playhead. It replaces a LENGTH readout.
- **Two rows of eight steps**, each a number, the page knob, the anchor button and its gate output on a plate. The second row's plate lines up with the CV/GATE plate.

## Open questions

- [ ] What the display shows: all three pages as thin bars, or only the current page as wider ones. It is the first W72 display that plots values, so it also settles the open question about small screens in [design-system.md](../design-system.md).
- [ ] The default for *Gate outputs*, trigger or gate.
- [ ] VALUE's range before RANGE and OFFSET, and their ranges.
- [ ] How the loop draws a new value (uniform, a random walk, Turing bits), and whether the choice is a menu item.
- [ ] Whether CV moves on a step whose gate doesn't fire, or holds.
- [ ] What KEEP a step returns to when its anchor is released.
- [ ] Relative steps (each step a move from the last note), set aside for later.
