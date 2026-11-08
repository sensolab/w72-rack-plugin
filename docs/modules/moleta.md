# Moleta

*A clock whose four outputs each skip beats by chance, and can lock what they skipped into a phrase.*

## What it does

Moleta is a clock you patch into drums, envelopes and sequencers. Each of its four outputs ticks at its own speed, and each one can drop some of its ticks at random. What you hear is a groove that breathes: hats that leave gaps, a bass that sometimes skips a bar.

Three knobs shape each output. RATIO sets how fast it ticks against the beat, from once every 32 beats to 32 times a beat. A few fractions, like 3/2, give cross-rhythms. CHANCE sets how many ticks get through. LENGTH turns the ticks into short triggers or longer gates.

Random gaps are fun until you find one you like. Turn REPEAT up and Moleta starts repeating its choices. All the way right, the pattern locks into a 16-step phrase. Once it's locked, CHANCE still thins or fills it without reshuffling it.

The obvious job is drums, but any ticks will do. Clock a sample and hold for stepped modulation that changes only sometimes, or gate an envelope for swells that come and go. Moleta runs on its own tempo, or follows a clock you patch into CLOCK. It is 18 HP wide, and needs nothing patched in to start.

## The panel

![Moleta panel](moleta.png)

| Control | What it does |
| --- | --- |
| Readout | The tempo, then each output's ratio. Shows `---` until an external clock has sent two beats. A chevron (›) between two ratios means the output on the right follows the one on its left. |
| RUN | Starts and stops the clock. Stopped, every output is silent. |
| BPM | The tempo, 30 to 300 BPM. Ignored while CLOCK is patched. |
| RESET | Restarts every output and every pattern from the top. |
| RATIO 1 to 4 | How fast the output ticks: ×2 is twice a beat, /4 once every four beats, 3/2 three ticks every two beats. |
| REPEAT | Fully left, every tick is a fresh coin flip. Fully right, the pattern locks. In between, it slowly changes. |
| DENSITY input | CV that adds to or takes from every CHANCE at once. Mono. |
| CHANCE 1 to 4 | How often a tick gets through. Fully right, always. |
| LENGTH 1 to 4 | Fully left, a short trigger. Further right, a gate that lasts longer, up to almost the next tick. |
| RUN input | A trigger starts or stops the clock. |
| CLOCK input | A clock to follow instead of BPM. Optional. |
| RESET input | A trigger resets, like the button. |
| CLICK | Audio: a metronome click, accented on the first beat of the bar. |
| BEAT | A trigger on every beat, never skipped. |
| RESET output | A trigger whenever Moleta resets. Patch it into a sequencer's reset to keep the two in step. |
| 1 to 4 | The ticks that got through. Mono. |

## Right-click options

The defaults suit a clock at one pulse per beat in 4/4.

| Option | What it does | Default |
| --- | --- | --- |
| Clock input | Set to 4 or 24 if the clock in CLOCK sends that many pulses per beat. | 1 |
| Beats per bar | Where the CLICK accent falls. | 4 |
| Follow the output above | Outputs 2 to 4 only play while the output above them is playing. Rests then fall on whole beats or bars. | Off |

## Samples

### Start here: a skipping pulse

1. Add Moleta, VCV's VCO, ADSR, VCA and Audio.
2. Patch Moleta's output **1** into the ADSR's gate, and the ADSR into the VCA's CV.
3. Patch the VCO's sine through the VCA into Audio. You hear a blip twice a beat.
4. Turn **CHANCE 1** down to about half. Now some blips drop out.
5. When you like the gaps, turn **REPEAT** fully right.

You now have a rhythm that skipped at random, then settled into a phrase.

### More ideas

**Thinned hats.** Output 1 at ×4 and 70% into a hi-hat. Turn LENGTH fully left so each tick is a trigger.

**Bars that breathe.** Output 1 at /4 and 60%. Output 2 at ×2, with *Follow the output above* on. When output 1 drops a bar, output 2 rests for the whole bar too.

**Modulation that waits.** Output 3 at /2 and 40% into a sample and hold. The value changes only now and then.

**When it won't follow your clock.** Moleta ignores BPM while CLOCK is patched. If the readout shows `---`, the clock in CLOCK hasn't sent two beats yet. If the tempo reads four times too fast, set *Clock input* to 4.

<!-- Sample patch to come: built in Rack with VCV Fundamental modules only, saved to patches/. -->

## Where it comes from

<!-- Photo to come: a historical photo of a moleta, the knife grinder, at his wheel, public domain or CC-licensed, with its credit. -->

*Moleta: the itinerant knife grinder. He pedals his wheel at a steady pace, and the sparks fly off it wherever they like.*

I kept patching the same pair: a clock, then a Chances module on an output, to drop some of its pulses. It's in 29 of my 175 saved patches, sometimes four Chances at once. I never touched their probability CV. I set it by ear and left it. So Moleta puts the chance on the output itself.

The clock modules I used had either fractions or chance, never both, and none let me pick a trigger or a gate per output. SickoCV Clocker has integer ratios and widths. CVfunk Syncro has fractions but no chance. Count Modula Chances flips a coin on any signal. ALM's Pamela's Workout does ratio, width and chance per output in hardware, and is the closest ancestor.

Moleta + adds four more outputs, and up to four chain for twenty in all; see [its manual](moletaplus.md). Timing details, edge cases and the saved state are in the [reference](moleta-reference.md).
