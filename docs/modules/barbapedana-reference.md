# Barbapedana reference

The details behind [Barbapedana's manual](barbapedana.md): exact ranges, timing, edge cases and what a patch saves. You don't need any of it to play the module.

## Every control in detail

| Control | What it does |
| --- | --- |
| PAGE | Which 16 of the 48 step parameters the knobs show: VALUE, KEEP or CHANCE. A three-way switch. Each page has its own parameters and PAGE only shows one set, so undo, Randomize, presets, MIDI maps and Brumista all see stable parameters (KEEP 5 is always KEEP 5), and a MIDI map to a hidden page still works. |
| LENGTH | Steps played, 1 to 16 (default 16). Lowering it past the playhead wraps to step 1 on the next clock. |
| MUTATE | The chance, 0 to 100%, that a free step's loop value is redrawn as the playhead reaches it (default 10%). |
| RANGE | The span of the CV, 0 to 10 V (default 2 V). |
| OFFSET | The bottom of the span, −5 to +5 V (default 0). |
| CLOCK | One step per rising edge (Schmitt trigger, 0.1 V and 1 V). |
| RESET | A rising edge makes the next clock play step 1; a clock in the same sample plays step 1. |
| MUTATE input | Added to MUTATE, 10% per volt. |
| CHANCE input | Added to every step's CHANCE, 10% per volt. |
| TURING | Patched, a redraw samples this input instead of drawing (see *The loop*). |
| KEEP input | Added to every free step's KEEP, 10% per volt. Anchored steps ignore it. |
| CV | OFFSET + value × RANGE, where value is the step's VALUE or the loop's value (0 to 1). |
| GATE | 10 V while the gate is open. |

Each step:

| Control | What it does |
| --- | --- |
| VALUE | 0 to 1 (default 0.5, the middle of the span). The tooltip shows the volts, OFFSET + value × RANGE, and typing a voltage sets the value that sends it. |
| KEEP | The chance, 0 to 100%, the step plays VALUE rather than the loop (default 0%, so a fresh module plays the loop). |
| CHANCE | The chance, 0 to 100%, the step fires its gate (default 100%). |
| Anchor | A latching button. Anchored, the step plays VALUE. See *Anchors*. |
| Output | The step's own gate, 10 V, with the same timing as GATE. |

The CVs' scaling is a starting point, to tune by ear. In the module browser the display shows a sample.

## Every menu option

| Item | Choices | Default |
| --- | --- | --- |
| Gate outputs | Gate (half a step) or trigger (1 ms) | Gate |
| CV on silent steps | Hold or move | Hold |
| Loop draws | Walk, anywhere or Turing bits | Walk |

## Timing and behaviour

- **A step.** On each clock the playhead moves on, wrapping at LENGTH. If the step reached is free, a coin with MUTATE's chance decides whether its loop value is redrawn. Then a coin with the step's KEEP (plus KEEP CV, clamped to 0 to 100%) picks VALUE or the loop's value, and a coin with CHANCE (plus CHANCE CV) decides whether the gate fires. An anchored step skips the redraw and the KEEP coin.
- **The loop.** 16 values, 0 to 1, one per step, filled at random when the module is made, reset or randomized. *Walk* moves a redrawn value by up to an eighth of the span either way, reflected at the ends so it never sticks there. *Anywhere* draws a new value across the whole span. *Turing bits* keeps a 16-bit circular register: step *k* reads the eight bits from bit *k* on, the first the most significant, as a value from 0 to 255/255, and a redraw at step *k* flips bit *k*. A flip moves step *k* by half the span and nudges the seven steps before it by less, so neighbours stay related. The register takes over the loop at its first flip after *Turing bits* is chosen.
- **TURING.** Patched, a redraw sets the step's loop value so that the step sends the voltage TURING carries at that moment: (TURING − OFFSET) / RANGE, clamped to 0 to 1 (0 when RANGE is 0). It overrides all three draws. The loop holds the value as a share of the span, so turning RANGE or OFFSET afterwards moves it with the rest of the line.
- **Anchors.** Pressing a free step's anchor on the panel writes the loop's value at that step into VALUE and anchors the step, as one undo action; pressing it again frees the step, also one undo action. The anchor never touches KEEP: a freed step plays by its KEEP knob again. Anchoring by any other way, a MIDI map or Brumista's `set_param`, locks the step without a capture.
- **The CV.** By default the CV only moves when a gate fires, so a note's release tail never jumps in pitch; *Move* makes it follow every step. While a step that played VALUE is held, turning its VALUE knob moves the CV at once. RANGE and OFFSET always apply live, to the note held as well.
- **Gates.** A gate lasts half the last clock interval, measured edge to edge, rounded to the nearest sample. Until two clocks have measured it, a gate lasts 100 ms. If a gate is still open when the next one fires (the clock sped up), the gate drops for one sample between them, so two notes stay two notes. *Trigger* makes every gate 1 ms. GATE and the step's own output open and close together.
- **Saved with the patch.** The loop's 16 values, the Turing register and the menu choices, so a loop reopens as it was. The playhead isn't saved: a reopened patch starts at step 1 on the first clock.

## Patch notes

- **Catch a phrase:** every KEEP at 0%, MUTATE fully left, then anchor the steps whose notes you like and raise MUTATE to about a third.
- **A program with give:** every KEEP at 100%, a few pulled to 50%, MUTATE low on *Walk*: the line keeps its shape and bends here and there.
- **Fate on a CV:** an envelope or slow LFO into KEEP CV, from the program to the loop and back.
- **A rhythm layer:** CHANCE low on the off-beats, a slow random into CHANCE CV.
- **Gates as drums:** a few step outputs into drum voices; the line's rhythm drives them.

## Saved state

What Barbapedana saves beyond its parameters, as Brumista's `get_module_data` reads it:

| Key | Holds |
| --- | --- |
| `loop` | The loop's 16 values, 0 to 1 |
| `bits` | The Turing register, 0 to 65535 |
| `gateMode` | 0 gate, 1 trigger |
| `silentMode` | 0 hold, 1 move |
| `draw` | 0 walk, 1 anywhere, 2 Turing bits |

A missing or unknown value loads as its default.

## Background and prior art

In the saved patches, melodies come from random loops and a quantizer far more often than from programmed steps: OrangeLine Mother is in 62 of 182 patches, SickoCV RandLoops8, Vult Dopamine and OrangeLine Morph follow, while plain step sequencers appear in a handful (Fundamental SEQ3 in 5, Impromptu Gate-Seq-64 in 6).

| Module | What it does | How Barbapedana differs |
| --- | --- | --- |
| OrangeLine Morph / Morpheus | Morphs between a given or stored sequence and a random one, one LOCK chance for the loop | KEEP per step, anchors, and capture into the knobs |
| Bruer SEQ1 | Probabilistic Turing sequencer with a quantizer and Euclidean triggers | Programmed steps beside the loop; no quantizer |
| Animatek UZZ, Count Modula Multi-step | 16 steps with a probability per step | The probability chooses between your value and the loop, besides firing the gate |
| OhmerPrems FranKe | Multi-track step sequencer with Turing machines inside | One line where each step is part yours, part the loop's |
| InfiniteNoise Turing Machine | Turing machine with 16 gate outputs | Gate outputs per step, on a sequence you can program |
| SickoCV RandLoops8, Grayscale Permutation | Turing-style looping random voltages | A program to hold the loop against |

## Panel geometry

Barbapedana is 30 HP, on a grid of 2 px columns: a 96 px section of shared controls (two columns 48 px apart) and, past a split, the steps 44 px apart. The steps' second row sits on the bottom plate beside CV and GATE; the first row has a plate of its own, as tall as the bottom one, across the step section. The display is a plot two rows tall, across the step section. RANGE and OFFSET line up with the first row of step knobs. `panels/Barbapedana.json` describes it.

## Open questions

- [ ] Relative steps (each step a move from the last note), set aside for later.
- [ ] The CVs' scaling, MUTATE's default and the first gate's 100 ms, by ear.
