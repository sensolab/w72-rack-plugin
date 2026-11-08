# Gibigiana reference

The details behind [Gibigiana's manual](gibigiana.md): exact ranges, timing, edge cases and what a patch saves. You don't need any of it to play the module.

## Every control in detail

| Control | What it does |
| --- | --- |
| ADD LANE | Places a Gibigiana + after the last lane of the chain and pushes the modules there to the right, in one undo action. At 8 lanes it does nothing. Momentary, lit while held. |
| CENTRE | The centre of the band every lane goes through, 20 Hz to 20 kHz; the middle is about 630 Hz. |
| WIDTH | The band's width. Fully right, the default, the band is open and does nothing; turning it left narrows a high-pass and a low-pass around CENTRE, down to one frequency at the far left. |
| REPEAT | How often the coin loops are redrawn, as on Moleta (see below). One knob for every lane. |
| DUCK | How far the DUCK input pulls the reflections down, 0 to 100% (default 80%). |
| IN | The voice, mono. Reads the first channel. |
| CLOCK | One step per rising edge. Required: unpatched, the lanes fall silent. |
| GATE | Each rising edge is a hit and rolls the coins. Unpatched, every CLOCK pulse is a hit. |
| RESET | A trigger starts every coin loop from its first number. |
| DENSITY | CV added to every lane's CHANCE, 10% per volt. |
| DUCK (input) | A sidechain, usually the anchor voice's audio. |
| L, R | The stereo sum of every lane still in it, through a gentle soft clip that keeps it inside ±10 V. |

Lane 1, the same controls a [Gibigiana +](gibigianaplus-reference.md) carries:

| Control | What it does |
| --- | --- |
| Readout | The lane's step over its number, 1. A step drawn dim reaches past the 20 s buffer at the current tempo, and the lane is silent. |
| STEP | How many clock steps behind the input the lane reads, in 14 steps: ⅓, ½, ⅔, 1, 1⅓, 1½, 2, 2⅔, 3, 4, 6, 8, 12 and 16. Default 1½. |
| CHANCE | The probability that a hit comes back in this lane, 0 to 100%. |
| LEVEL | The lane's level, from silent to 0 dB; the middle, the default, is -6 dB. |
| PAN | The lane's place in the stereo sum, equal power, centred by default. |
| CHANCE input | Added to CHANCE, 10% per volt, with DENSITY. |
| LEVEL input | Added to LEVEL, 10% of its range per volt. |
| PAN input | Added to PAN, 20% of its range per volt: ±5 V sweeps it from side to side. |
| TAP | The lane alone, mono, after the band, LEVEL and the duck. Patched, it leaves the sum unless *Patched tap stays in the sum* is on. |

The lane CVs' scaling is a starting point, to tune by ear. In the module browser the readout shows a sample, 1½.

## Every menu option

| Item | Choices | Default |
| --- | --- | --- |
| Add lane | As ADD LANE; greyed at 8 lanes | |
| Coin loop | 8 or 16 hits | 16 |
| Patched tap stays in the sum | On or off | Off |

## Timing and behaviour

- **The chain.** Lane 1 is Gibigiana's own; each Gibigiana + touching the chain on the right adds the next lane, up to 8. All of the sound is made in Gibigiana: each Gibigiana + sends its knobs and CVs leftward every sample and gets its own tap back. Rack passes data between neighbours one sample late, so lane *n* reads its tap *n* − 1 samples early and its TAP lands on the grid; its share of L and R is held back by as much, so the sum is on the grid too.
- **Lanes come and go.** A lane that leaves the chain fades out over 2 ms and stops. A lane that joins plays nothing written before it joined, so it stays silent until a hit has rolled its coin and that hit comes back.
- **The step.** The step is the time between CLOCK pulses: the mean of the last two intervals, so a swung clock reads as its average. A lane reads the buffer STEP steps behind the write head, rounded to the nearest sample. An interval more than two and a half steps long is taken as the clock having stopped and is ignored once; a second one in a row is the new tempo. The lanes keep their place while the clock is stopped, and fall silent when CLOCK is unpatched.
- **Tempo changes.** When the tempo or a STEP changes, the lane crossfades to its new place over 30 ms instead of gliding there, so the echoes don't warble. Changes smaller than clock jitter (a few samples, or 0.1%) are ignored.
- **Hits and coins.** On each hit every lane in the chain draws from its coin loop and decides whether this hit comes back there. The decision is written into the buffer next to the audio, and covers everything from that hit until the next one, so a note and its tail share one fate, and the decision travels with the sound to the lane that reads it. Before the first hit, nothing comes back. Each decision fades in or out over 2 ms that end exactly on the hit: a tail that isn't wanted is gone before the next note starts, and a note that is wanted keeps its attack whole.
- **The coin loop.** As on Moleta: each lane keeps a loop of 16 stored random numbers (8 with *Coin loop*) and a pointer. On each hit the number at the pointer is redrawn with probability 1 − REPEAT, the lane sounds if the number is below CHANCE plus its CV plus DENSITY (clamped to 0 to 100%), and the pointer moves on. Fully right, the reflections repeat as a phrase of 16 (or 8) hits; lowering CHANCE thins that phrase without reshuffling it. RESET starts every loop from its first number. The loops belong to the places in the chain, not to the modules, and Gibigiana keeps all eight.
- **The buffer.** A fixed 20 s, sized in seconds rather than tempo: 16 quarter-note steps down to 48 BPM, or 16 eighths down to 24 BPM. It takes about 4.8 MB at 48 kHz. A lane that would reach past it falls silent rather than wrapping, and its step dims on the readout. Changing the sample rate empties it.
- **The band.** CENTRE and WIDTH set one band for every lane: a 12 dB/octave high-pass and low-pass, WIDTH × 5 octaves either side of CENTRE. Each lane is filtered on its own after it is read, before LEVEL and pan.
- **The duck.** The DUCK input goes through an envelope follower with a fixed 2 ms attack and 150 ms release. At 5 V and above it pulls every lane down by the whole DUCK amount; the TAP outputs and the sum are both ducked.
- **The sum.** L and R carry the panned lanes that are still in the sum, from every lane in the chain. Up to 5 V the sum is untouched; above it, it bends smoothly towards 10 V.
- **Saved with the patch.** Lane 1's step, the coin loops of all eight places (their numbers and pointers) and the menu choices, so a locked phrase reopens as it was. Each Gibigiana + saves its own step. The buffer itself isn't saved.

## Patch notes

- **Kick-keyed echo lane:** a sparse percussion voice into IN, the kick's dry output into DUCK, L and R on their own mixer channel. The echo lives in the gaps the kick leaves.
- **Dotted counterpoint:** a melody into IN with GATE from its sequencer, one lane at 1½ and one at 3, both at 100%: the line answers itself off the beat.
- **Ghost notes:** four lanes at ⅓, ⅔, 1⅓ and 2⅔ at low CHANCE and LEVEL, with REPEAT right once a pattern sits well.
- **One lane, its own processing:** patch lane 4's TAP into a filter or a pitch shifter and back into the mix; it leaves the sum, the other lanes stay.
- **A wash:** L and R into Prince Of Perception or a reverb. Gibigiana makes the reflections; the tail comes from downstream.

## Saved state

What Gibigiana saves beyond its parameters, as Brumista's `get_module_data` reads it:

| Key | Holds |
| --- | --- |
| `step` | `{"p", "q"}`: lane 1's step itself, p/q clock steps. On load, the STEP knob goes to the step with that value, or the nearest one. |
| `coins` | Eight objects, lanes 1 to 8 by place in the chain: `slots`, the 16 stored numbers (0 to 1), and `pointer`, the next slot |
| `coinLoop` | 8 or 16 |
| `keepPatched` | Whether a patched tap stays in the sum |

A missing or unknown value loads as its default.

## Background and prior art

Delay is where the saved patches compose. In a scan of 176 patches, Prince Of Perception appears in 99, two or more of them in 43. Its instances are clocked in 163 of 165, ducked in 116, and nearly always fully wet on a mixer channel of their own, while its looper and freeze controls are never touched. Seven of the technique cards are echo techniques, and five of them (a kick-keyed echo lane, a high-passed accent echo, dotted counterpoint, loop-locked heterophony, multitap ghost notes) want reflections on the grid with a chance, not a longer tail. The other two need feedback and stay with Prince and Prism.

| Module | What it does | How Gibigiana differs |
| --- | --- | --- |
| FLAG Prince Of Perception | Clocked delay with feedback, a DJ filter, a duck input and a looper | Up to eight taps, a chance per tap, no feedback |
| FrozenWasteland Portland Weather | 16 taps on a grid of the clock, pitch and filter per tap | Portland Weather can give each tap a chance with two expanders, rolled per clock. Gibigiana rolls per hit and carries the coin with the echo |
| 4ms Tapographic Delay | Taps placed by tapping, with a level per tap | Taps on clock steps, each with a chance |
| Submit Chrono | Three heads at clocked ratios, thirds included | A chance per tap, an output per tap |
| CVfunk Tri Delay | Three taps as offsets of one time | Clocked steps, a chance per tap |

## Panel geometry

Gibigiana is 10 HP: a 6 HP system section and lane 1, split by a hairline. Lane 1 is drawn as the very strip a Gibigiana + repeats, 90 px over, so the lanes read left to right in one rhythm. The panel sits on a grid of half-HP columns (7.5 px); the system's knobs are the equal shares of its 90 px, its jacks a 30 px row. What a lane is about, its STEP and CHANCE, are knobs; LEVEL and PAN are trimpots (see [design-system.md](../design-system.md#parts)). A rule on the plate, on the split, divides the sum from lane 1's TAP. `panels/Gibigiana.json` describes it.

## Open questions

- [ ] The duck's attack and release, and the soft clip's knee, by ear against Prince Of Perception.
- [ ] The lane CVs' scaling, by ear.
