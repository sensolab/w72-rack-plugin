# Ghisa

*Feed Ghisa a melody, and it plays chords made from the notes the melody just played.*

## What it does

Ghisa remembers the last 16 notes of a melody. On every clock it plays a few of them at once as a chord, on a polyphonic output. Because the notes come from your own melody, the chord always fits it, and it moves as the melody moves.

The eight buttons pick which of those remembered notes go into the chord. Light the first one and you get the note just played; light the third and you get the one from two clocks ago. That set of lit buttons is the voicing. DEPTH reads the same voicing further back in the memory, so the chord drifts back through what the melody played. DEPTH takes CV, so the voicing can move on its own.

BASS picks one note from the chord for a bass voice. CHANGE fires for each voice only when its note actually changes, so notes that stay in the next chord keep ringing instead of striking again. HOLD freezes the memory and loops it, so the chords repeat.

Ghisa has no quantizer. Feed it a melody that is already in a scale, and every chord stays in that scale. It doesn't have to be pitch, though: any voltage works, so it also turns a slow LFO into a set of related modulation voltages. Ghisa is 8 HP wide, and needs a signal in IN and a clock in CLOCK.

## The panel

![Ghisa panel](ghisa.png)

| Control | What it does |
| --- | --- |
| REGISTER display | The 16 remembered notes, newest at the left. Grey bars are the eight the buttons can reach, and bright bars are the ones in the chord. Turn DEPTH and both slide into the past. |
| Shape buttons | Eight buttons, read left to right, top row first. Each lit button adds one remembered note to the chord. |
| DEPTH | Reads the shape further back, up to 8 notes. At 0 the buttons cover notes 1 to 8; at 8, notes 9 to 16. |
| HOLD | Stops taking new notes and loops the ones it has. |
| DEPTH input | CV added to the DEPTH knob, one note further back per 1.25 V. |
| HOLD input | A gate: the memory loops while it is high. |
| IN | The melody, or any voltage. Mono: from a poly cable, only the first channel is recorded. |
| CLOCK | Each clock takes a new note and updates the chord. Required. |
| BASS CLK | Optional. Patch a slower clock here and the bass changes only on it. |
| CHORD | Poly: one channel per lit button. Patch it into a poly voice's V/Oct. |
| CHANGE | Poly: a trigger on each channel whose note changed. Patch it into the same voice's envelope. |
| BASS | Mono: one note of the chord, for a bass voice. |

Changes to the buttons and DEPTH land on the next clock, so the chord always changes in time.

## Right-click options

The defaults work. These are the ones worth knowing early.

| Option | What it does | Default |
| --- | --- | --- |
| Bass | *Lowest* plays the lowest note of the chord. *Root* plays the note the chord seems built on. | Lowest |
| Bass octave | Keeps the bass in a low register, whatever the melody's range. | Off |
| Chord range | Keeps the chord in one octave or two, whatever the melody's range. | Off |
| Chord width | How wide that range is: 1 or 2 octaves. | 1 octave |
| Chord order | Channels in button order, or low to high. | By stage |
| Duplicate notes | What to do when two buttons give the same note: keep it, skip it, or move it up an octave. | Keep |
| Change | *Trigger*, or *Gate until next clock* for envelopes that need a gate. | Trigger |
| Hold loop length | How many notes HOLD loops. Shorter loops repeat sooner. | 16 |

## Samples

### Start here: a pad that follows a melody

1. Add a sequencer and a clock (VCV's SEQ 3 and LFO work), then VCV's Quantizer, VCO, ADSR, VCA and Audio.
2. Patch the sequencer through the Quantizer into Ghisa's **IN**. Patch the clock into both the sequencer and Ghisa's **CLOCK**.
3. Patch **CHORD** into the VCO's V/Oct, and **CHANGE** into the ADSR's gate.
4. Run the VCO through the VCA into Audio, with the ADSR on the VCA's CV. You hear a three-note chord that moves with the melody.
5. Press a few shape buttons, then turn DEPTH slowly.

You now have a pad that voices itself from the melody, and a knob that moves the voicing.

### More ideas

**A bass under it.** Patch BASS into a mono voice. Patch a slower clock, like the beat, into BASS CLK, so the bass moves less often than the chord.

**A close pad from a wide melody.** If the chord jumps all over the keyboard, set *Chord range* to C3 or C4. The notes stay in one register and the voicing inverts as it moves.

**A phrase and its echo.** Patch a slow gate into HOLD. Ghisa takes new notes, then loops them while the gate is high. *Hold loop length* at 8 makes an eight-note repeat easy to line up.

**Drifting voicing.** A slow random CV into DEPTH, with the knob near the middle. The pad wanders back through what the melody played.

**When the chord goes thin.** A melody that repeats every few notes can put the same note on several buttons. Set *Duplicate notes* to *Skip*, or light a different button.

<!-- Sample patch to come: built in Rack with VCV Fundamental modules only, saved to patches/. -->

## Where it comes from

<!-- Photo to come: a historical photo of a ghisa, a Milanese traffic warden, on his platform, public domain or CC-licensed, with its credit. -->

*Ghisa: the Milanese traffic warden. It stands in the stream of notes and sends each one where the chord needs it.*

I kept building the same patch by hand. A quantizer fed a shift register, I merged a few of its stages into a pad, and cvly bss picked a bass note from it. Which stages made the chord depended on which jacks I had patched, so the voicing never moved. Ghisa turns that choice of stages into buttons, and moving it into a CV.

Other shift registers give a fixed jack per stage, like ML Shift Register, Count Modula ASR and VCV SHASR. SickoCV shifter has one movable tap, and Stoermelder SIPO evenly spaced poly taps. AH Bombe and Tyrant write chords for you. Ghisa reads them from what you played.

The *Root* bass uses the interval scores of cvly bss, from cvly-modules by Lyqst (GPL-3.0).

Timing details, edge cases and the saved state are in the [reference](ghisa-reference.md).
