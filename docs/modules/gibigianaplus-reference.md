# Gibigiana + reference

The details behind [Gibigiana +'s manual](gibigianaplus.md): exact ranges, timing, edge cases and what a patch saves. You don't need any of it to play the module.

## Every control in detail

| Control | What it does |
| --- | --- |
| Readout | The lane's step over its number in the chain, 2 to 8. Drawn dim, with no number, when no Gibigiana heads the chain or the lane is past the eighth; drawn dim when the step reaches past Gibigiana's 20 s buffer at the current tempo. |
| STEP | How many clock steps behind Gibigiana's input the lane reads, in 14 steps: ⅓, ½, ⅔, 1, 1⅓, 1½, 2, 2⅔, 3, 4, 6, 8, 12 and 16. |
| CHANCE | The probability that a hit comes back in this lane, 0 to 100%. Gibigiana's DENSITY is added to it. |
| LEVEL | The lane's level, from silent to 0 dB; the middle, the default, is -6 dB. |
| PAN | The lane's place in Gibigiana's stereo sum, equal power, centred by default. |
| CHANCE input | Added to CHANCE, 10% per volt. |
| LEVEL input | Added to LEVEL, 10% of its range per volt. |
| PAN input | Added to PAN, 20% of its range per volt: ±5 V sweeps it from side to side. |
| TAP | The lane alone, mono, after Gibigiana's band, LEVEL and duck. Patched, it leaves Gibigiana's sum unless Gibigiana's *Patched tap stays in the sum* is on. 0 V with no Gibigiana at the head of the chain. |

A new lane's STEP follows its place in the chain: 3 for lane 2, 4 for lane 3, 8 from lane 4 on. ADD LANE sets it; a Gibigiana + placed from the module browser takes it the first time it joins a chain, unless its STEP has been turned. CHANCE starts at 100%, LEVEL at -6 dB and PAN in the middle.

In the module browser the readout shows a sample: 3, over 2.

## Timing and behaviour

- **The chain.** A Gibigiana + makes no sound of its own. Every sample it sends its knobs, its CVs and whether its TAP is patched to the module on its left, along with what it got from its right, and Gibigiana computes every lane. Gibigiana sends each lane's tap back down the chain, one module per sample, reading lane *n* *n* − 1 samples early so each TAP lands on the clock grid. Knob and CV changes reach Gibigiana a few samples late, which nothing can hear.
- **The cap.** A chain holds 8 lanes, Gibigiana's own and seven Gibigiana +. A ninth is silent, with its readout dim.
- **Joining and leaving.** A lane that leaves the chain fades out over 2 ms. A lane that joins stays silent until Gibigiana's next hit has rolled its coin and that hit comes back.
- **Its coins.** The coin loops belong to the places in the chain and are kept by Gibigiana. Delete lane 3 and push lane 4 over, and it plays lane 3's pattern with its own knobs.
- **No Gibigiana.** With no Gibigiana at the head of its chain, or a module of another kind in between, the TAP is 0 V and the readout dims.
- **Saved with the patch.** The lane's step.

## Saved state

What Gibigiana + saves beyond its parameters, as Brumista's `get_module_data` reads it:

| Key | Holds |
| --- | --- |
| `step` | `{"p", "q"}`: the step itself, p/q clock steps. On load, the STEP knob goes to the step with that value, or the nearest one. |

A missing or unknown value loads as its default.

## Panel geometry

4 HP, the strip lane 1 has on Gibigiana: a readout, STEP and CHANCE knobs, LEVEL and PAN trimpots, the three CV inputs and TAP on the plate. Its two columns are 15 px in from each side, 30 px apart, the least a pair of jacks may have. Under 6 HP the mark would sit where the plate is, so the panel goes unsigned; Gibigiana signs the chain (see [design-system.md](../design-system.md#geometry)). `panels/GibigianaPlus.json` describes it.
