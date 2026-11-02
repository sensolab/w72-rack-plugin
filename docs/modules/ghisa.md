# Ghisa

*The Milanese traffic warden: it stands in the stream of notes and sends each one where the chord needs it.*

> **Designed, not built.** This page is the design. It becomes the manual when Ghisa ships, and gets its `manualUrl` then.

Ghisa is a shift register whose taps are a control. Each clock pushes the input's voltage into a 16-stage register. A shape of up to eight stages, drawn with buttons, reads the register into a polyphonic CHORD, and a DEPTH knob with CV slides that shape back through the register, so the same voicing reads from older memory. BASS gives one note of the chord for a bass voice.

- **Family:** trades · **Width:** 8 HP · **Slug:** `Ghisa` · **Since:** not released

## Why

It replaces a patch built by hand again and again, recorded as the *chord from delay taps* technique: a quantizer (OrangeLine Mother) feeds an analogue shift register (ML Shift Register, Count Modula ASR), a few stages that aren't next to each other are merged into a pad, and cvly `bss` takes the root or the lowest note of the merge for a bass. The chord programs itself from the melody's recent past.

The part that hurts is the cabling. Which stages make the chord is set by which jacks are patched, so the voicing can't move or be modulated. Ghisa makes the choice of taps a control, and the movement of that choice a CV.

Ghisa has no quantizer. The quantizer upstream is where the harmony is decided (scale, child scale, weights, fate), and a built-in one would duplicate it and lose. Fed a quantized stream, every tap is in scale.

## Prior art

| Module | What it does | How Ghisa differs |
| --- | --- | --- |
| SickoCV shifter | 64 stages, one output whose stage takes CV | One tap; Ghisa moves a whole voicing |
| Stoermelder SIPO | Shift register with a poly output, CV for skip and step | Evenly spaced taps; Ghisa's shape is drawn freely |
| ML Shift Register, Count Modula ASR, Fundamental SHASR | Fixed outputs per stage | Taps are cables |
| AH Bombe, StudioSixPlusOne Tyrant | Shift-register chord generators | They write the chord; Ghisa reads it from what you played |

## How it works

- **Register.** 16 stages. On each rising edge at CLOCK, every stage passes its voltage one stage down and stage 1 samples IN. Stage 1 is the newest. IN reads its first channel. The register holds any voltage, so it works on pitch, modulation or slow CV alike.
- **Shape.** Eight latching buttons, each with a square light. Button *k* lit means the stage *k* places into the window is in the chord.
- **DEPTH.** Where the window starts: stage 1 + DEPTH, from 0 to 8, in whole stages. The knob and the CV add (1.25 V per stage, so 0 to 10 V covers the range) and the sum is clamped. At DEPTH 0 the window is stages 1 to 8; at DEPTH 8 it is stages 9 to 16.
- **Changes land on the clock.** A new shape or DEPTH takes effect at the next clock, together with the shift, so the chord only ever changes in time.
- **HOLD.** While the button is on or the HOLD gate is high, each clock recirculates stage 16 into stage 1 instead of sampling IN, so the register loops its last 16 values: a fixed point under the moving voicing.
- **Saved with the patch.** The register's contents are saved, so a patch reopens with the chord it closed on, and Brumista's `get_module_data` can read them.

## Outputs

| Output | Carries |
| --- | --- |
| CHORD | One channel per lit button, holding that tap's voltage. No button lit gives zero channels. |
| BASS | One voltage taken from the tapped stages, by the bass mode. |

Both change only on a clock.

## Right-click menu

The working modes live in the menu, not on the panel.

| Item | Choices | Default |
| --- | --- | --- |
| Bass | Lowest: the lowest tapped voltage. Root: the tap that best explains the others as a chord (thirds and fifths above it), as `bss`'s Harmonic mode does | Lowest |
| Bass octave | Off, or fold the bass into the octave above C1, C2 or C3 | Off |
| Chord order | By stage (button order) or by pitch, low to high | By stage |

## Panel

![Ghisa panel mock](ghisa-mock.png)

From the top: the register display, the shape buttons in two rows of four, DEPTH and HOLD, the inputs (IN, CLOCK, DEPTH, HOLD) and the plate with CHORD and BASS.

- **Register display:** sixteen flat bars, one per stage, newest at the left. Off: outside the window. Dim: in the window, not tapped. Full: tapped. The voicing can be watched moving.
- **Shape buttons and HOLD:** the square flat light from [design-system.md](../design-system.md#displays), on a button.
- The generator gains `button` (a latching button with a square light) and `bars` kinds with Ghisa.

## In a patch

- **Self-voicing pad:** a quantized melody into IN, the melody's clock (or a division of it) into CLOCK, CHORD into a poly voice, BASS into a mono bass voice.
- **Moving the voicing:** a slow stepped CV into DEPTH reads the same shape from deeper memory, so the pad drifts back through what the melody played.
- **Avoid the collapse.** If the source loops, taps a multiple of the loop's length apart read the same note and the chord thins to one pitch class. Use a loop length coprime with the shift period, randomize the source's clock, or move DEPTH.

## Open questions

- [ ] Panel layout: the register under the band (B on the mock) or one row lower (A).
- [ ] HOLD's loop length: always 16, or the deepest tapped stage, so the loop is as long as the chord's memory.
- [ ] Default shape: 1, 3, 5 is a guess, to be set by ear.
- [ ] Whether a CHANGE trigger out (high when the chord's notes change) earns its place.
