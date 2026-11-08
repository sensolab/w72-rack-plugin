# Moleta + reference

The details behind [Moleta +'s manual](moletaplus.md): exact ranges, timing, edge cases and what a patch saves. You don't need any of it to play the module.

## Every control in detail

| Control | What it does |
| --- | --- |
| Readout | Each output's ratio over its number, 5 to 8. |
| RATIO 5 to 8 | Each output's ratio, in the same 67 steps as Moleta: every divider from /32 to /2, then 2/3, 3/4, ×1, 4/3, 3/2, then every multiplier from ×2 to ×32. |
| CHANCE 5 to 8 | The probability that a pulse passes, 0 to 100%. Moleta's DENSITY is added to it. |
| LENGTH 5 to 8 | Fully left, a 1 ms trigger (*Trigger* in the tooltip). Further right, a gate from 5% to 95% of the output's own period; the middle is 50%. |
| 5 to 8 | The pulses that passed, 10 V, as triggers or gates by LENGTH. 0 V with no Moleta on the left. |

A fresh Moleta + runs at ×4, ×1, /8 and 3/2 on outputs 5 to 8, every pulse passes, and each is a gate half its own period long.

In the module browser the readout shows a sample: /2, ×4, 2/3 and /8.

## Timing and behaviour

- **The link.** Every sample Moleta hands its expander the beat clock's state (whether it is running, a beat, a reset, the beat count and the period T) and its REPEAT, DENSITY and *Coin loop*. Moleta + runs the same output engine as Moleta on that, so ratios, cycle restarts, the coin and LENGTH behave exactly as on outputs 1 to 4 ([Moleta's reference](moleta-reference.md#timing-and-behaviour) has the details).
- **Alignment.** Rack passes data between neighbours one sample late. While Moleta + is attached, Moleta delays all its own outputs (1 to 4, BEAT, RESET and CLICK) by one sample, so the eight outputs land together. Without it, Moleta has no delay.
- **Its own coins.** Each of outputs 5 to 8 keeps its own loop of 16 stored numbers and pointer. Moleta's RESET and RUN restart them with Moleta's own, and its REPEAT and *Coin loop* apply to them.
- **Follow the output above.** Outputs 6, 7 and 8 can follow the output to their left on Moleta +, as on Moleta. Output 5 does not follow Moleta's output 4: it starts a fresh chain.
- **No Moleta on the left.** If the module on the left is not a Moleta, or there is none, the outputs are 0 V. Placed back next to a Moleta, each output waits for its next cycle restart, so it falls in line with Moleta's beats; its coin loops are kept.
- **Saved with the patch.** The ratios, each output's coin loop (its numbers and pointer) and the follow choices.

## Every menu option

| Item | Choices | Default |
| --- | --- | --- |
| Follow the output above | Per output, 6 to 8: on or off | Off |

The clock, *Beats per bar* and *Coin loop* are set on Moleta.

## Patch notes

- **Eight thinned clocks:** Moleta's four on drums, Moleta +'s four on a melody line and its accents, all locked by one REPEAT.
- **A slow layer:** output 7 at /8 with a long LENGTH into a filter envelope, output 8 at 3/2 for a cross-rhythm against Moleta's ×2.
- **Cascading rests:** output 5 at /4 and 60%, outputs 6 to 8 at ×2, ×3 and ×4 each following the one above, so silence spreads from bar to beat to subdivision.

## Saved state

What Moleta + saves beyond its parameters, as Brumista's `get_module_data` reads it:

| Key | Holds |
| --- | --- |
| `ratios` | Four objects `{"p", "q"}`, outputs 5 to 8: the ratio itself. On load, each RATIO knob goes to the step with that ratio, or the nearest one. |
| `coins` | Four objects, outputs 5 to 8: `slots`, the 16 stored numbers (0 to 1), and `pointer`, the next slot |
| `follow` | Three booleans, outputs 6 to 8 |

A missing or unknown value loads as its default.
