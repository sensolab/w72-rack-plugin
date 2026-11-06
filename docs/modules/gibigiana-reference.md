# Gibigiana reference

The details behind [Gibigiana's manual](gibigiana.md): exact ranges, timing, edge cases and what a patch saves. You don't need any of it to play the module.

## Every control in detail

| Control | What it does |
| --- | --- |
| Readout | Each tap's step over its number. A step drawn dim reaches past the 20 s buffer at the current tempo, and that tap is silent. |
| STEP 1 to 4 | How many clock steps behind the input the tap reads, in 14 steps: ⅓, ½, ⅔, 1, 1⅓, 1½, 2, 2⅔, 3, 4, 6, 8, 12 and 16. |
| CHANCE 1 to 4 | The probability that a hit comes back at this tap, 0 to 100%. |
| LEVEL 1 to 4 | The tap's level, from silent to 0 dB; the middle, the default, is -6 dB. |
| PAN 1 to 4 | The tap's place in the stereo sum, equal power, centred by default. |
| CENTRE | The centre of the band every tap goes through, 20 Hz to 20 kHz; the middle is about 630 Hz. |
| WIDTH | The band's width. Fully right, the default, the band is open and does nothing; turning it left narrows a high-pass and a low-pass around CENTRE, down to one frequency at the far left. |
| REPEAT | How often the coin loops are redrawn, as on Moleta (see below). One knob for all taps. |
| DUCK | How far the DUCK input pulls the reflections down, 0 to 100% (default 80%). |
| IN | The voice, mono. Reads the first channel. |
| CLOCK | One step per rising edge. Required: unpatched, the taps fall silent. |
| GATE | Each rising edge is a hit and rolls the coins. Unpatched, every CLOCK pulse is a hit. |
| RESET | A trigger starts every coin loop from its first number. |
| DENSITY | CV added to every CHANCE, 10% per volt. |
| DUCK (input) | A sidechain, usually the anchor voice's audio. |
| L, R | The stereo sum of the taps still in it, through a gentle soft clip that keeps it inside ±10 V. |
| 1 to 4 | Each tap alone, mono, after the band, LEVEL and the duck. Patched, a tap leaves the sum unless *Patched tap stays in the sum* is on. |

In the module browser the readout shows a sample: 1½, 3, ⅔ and 8.

## Every menu option

| Item | Choices | Default |
| --- | --- | --- |
| Coin loop | 8 or 16 hits | 16 |
| Patched tap stays in the sum | On or off | Off |

## Timing and behaviour

- **The step.** The step is the time between CLOCK pulses: the mean of the last two intervals, so a swung clock reads as its average. A tap reads the buffer STEP steps behind the write head, rounded to the nearest sample. An interval more than two and a half steps long is taken as the clock having stopped and is ignored once; a second one in a row is the new tempo. The taps keep their place while the clock is stopped, and fall silent when CLOCK is unpatched.
- **Tempo changes.** When the tempo or a STEP changes, the tap crossfades to its new place over 30 ms instead of gliding there, so reflections don't warble. Changes smaller than clock jitter (a few samples, or 0.1%) are ignored.
- **Hits and coins.** On each hit every tap draws from its coin loop and decides whether this hit comes back there. The decision is written into the buffer next to the audio, and covers everything from that hit until the next one, so a note and its tail share one fate, and the decision travels with the sound to every tap that reads it. Before the first hit, nothing comes back. Each decision fades in or out over 2 ms that end exactly on the hit: a tail that isn't wanted is gone before the next note starts, and a note that is wanted keeps its attack whole.
- **The coin loop.** As on Moleta: each tap keeps a loop of 16 stored random numbers (8 with *Coin loop*) and a pointer. On each hit the number at the pointer is redrawn with probability 1 − REPEAT, the tap sounds if the number is below CHANCE plus DENSITY (clamped to 0 to 100%), and the pointer moves on. Fully right, the reflections repeat as a phrase of 16 (or 8) hits; lowering CHANCE thins that phrase without reshuffling it. RESET starts every loop from its first number.
- **The buffer.** A fixed 20 s, sized in seconds rather than tempo: 16 quarter-note steps down to 48 BPM, or 16 eighths down to 24 BPM. It takes about 4.8 MB at 48 kHz. A tap that would reach past it falls silent rather than wrapping, and its step dims on the readout. Changing the sample rate empties it.
- **The band.** CENTRE and WIDTH set one band for all taps: a 12 dB/octave high-pass and low-pass, WIDTH × 5 octaves either side of CENTRE. Each tap is filtered after it is read, before LEVEL and pan.
- **The duck.** The DUCK input goes through an envelope follower with a fixed 2 ms attack and 150 ms release. At 5 V and above it pulls the taps down by the whole DUCK amount; the tap outputs and the sum are both ducked.
- **The sum.** L and R carry the panned taps that are still in the sum. Up to 5 V the sum is untouched; above it, it bends smoothly towards 10 V.
- **Saved with the patch.** The steps, each tap's coin loop (its numbers and pointer) and the menu choices, so a locked phrase reopens as it was. The buffer itself isn't saved.

## Patch notes

- **Kick-keyed echo lane:** a sparse percussion voice into IN, the kick's dry output into DUCK, L and R on their own mixer channel. The echo lives in the gaps the kick leaves.
- **Dotted counterpoint:** a melody into IN with GATE from its sequencer, one tap at 1½ and one at 3, both at 100%: the line answers itself off the beat.
- **Ghost notes:** four taps at ⅓, ⅔, 1⅓ and 2⅔ at low CHANCE and LEVEL, with REPEAT right once a pattern sits well.
- **One tap, its own processing:** patch tap 4 into a filter or a pitch shifter and back into the mix; it leaves the sum, the other three stay.
- **A wash:** L and R into Prince Of Perception or a reverb. Gibigiana makes the reflections; the tail comes from downstream.

## Saved state

What Gibigiana saves beyond its parameters, as Brumista's `get_module_data` reads it:

| Key | Holds |
| --- | --- |
| `steps` | Four objects `{"p", "q"}`, taps 1 to 4: the step itself, p/q clock steps. On load, each STEP knob goes to the step with that value, or the nearest one. |
| `coins` | Four objects, taps 1 to 4: `slots`, the 16 stored numbers (0 to 1), and `pointer`, the next slot |
| `coinLoop` | 8 or 16 |
| `keepPatched` | Whether a patched tap stays in the sum |

A missing or unknown value loads as its default.

## Background and prior art

Delay is where the saved patches compose. In a scan of 176 patches, Prince Of Perception appears in 99, two or more of them in 43. Its instances are clocked in 163 of 165, ducked in 116, and nearly always fully wet on a mixer channel of their own, while its looper and freeze controls are never touched. Seven of the technique cards are echo techniques, and five of them (a kick-keyed echo lane, a high-passed accent echo, dotted counterpoint, loop-locked heterophony, multitap ghost notes) want reflections on the grid with a chance, not a longer tail. The other two need feedback and stay with Prince and Prism.

| Module | What it does | How Gibigiana differs |
| --- | --- | --- |
| FLAG Prince Of Perception | Clocked delay with feedback, a DJ filter, a duck input and a looper | Four taps, a chance per tap, no feedback |
| FrozenWasteland Portland Weather | 16 taps on a grid of the clock, pitch and filter per tap | Portland Weather can give each tap a chance with two expanders, rolled per clock. Gibigiana rolls per hit and carries the coin with the echo |
| 4ms Tapographic Delay | Taps placed by tapping, with a level per tap | Taps on clock steps, each with a chance |
| Submit Chrono | Three heads at clocked ratios, thirds included | A chance per tap, an output per tap |
| CVfunk Tri Delay | Three taps as offsets of one time | Clocked steps, a chance per tap |

## Panel geometry

The panel follows the trimpot rule widened for it (see [design-system.md](../design-system.md#parts)): what a tap is about, its STEP and CHANCE, are knobs; LEVEL, PAN and the shared settings are trimpots. The tap columns are 52.5 px apart, 26.25 px in from the edges, the equal shares of four; the LEVEL and PAN pairs are the equal shares of eight. A hairline divides the tap rows from the shared ones, and a rule on the plate divides the sum from the tap outputs. `panels/Gibigiana.json` describes it.

## Open questions

- [ ] The duck's attack and release, and the soft clip's knee, by ear against Prince Of Perception.
- [ ] LEVEL with CV, after playing it in Rack.
- [ ] Taps 5 to 8, or a send and return per tap, as an expander.
