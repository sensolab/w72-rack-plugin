# Moleta and Moleta+

Implementation plan for [#51](https://github.com/sensolab/w72-rack-plugin/issues/51) (Moleta, the probability master clock) and [#52](https://github.com/sensolab/w72-rack-plugin/issues/52) (Moleta+, its four-output expander), milestone 2.8.0.

[`docs/modules/moleta.md`](../modules/moleta.md) is the design and stays authoritative for behaviour: this plan says how to build it, and settles what the design leaves open. Where the two disagree, say so in the report rather than choosing silently.

## Delivery

Two pull requests, in order:

1. **#51, Moleta** on `feat/moleta`: the engine and its tests, the readout widget, the module, the expander-facing side, the manual.
2. **#52, Moleta+** on `feat/moleta-plus`, started from `feat/moleta` once that is reviewed: the expander module and its manual.

The expander is designed into #51 so that #52 only adds a consumer: the message format and the one-sample output delay live in the base module from the start, and #51 tests them with no expander attached.

## Not in scope

- Swing, BPM CV, an exact-time (sample-counting) metronome: the design leaves them out.
- Any change to `protocol/`, Brumista or Ghisa.
- The version bump (`chore(release)` comes after the milestone).
- Rack renders of the panels (`docs/modules/moleta.png`, `moletaplus.png`) and any test against a running Rack: both need the owner's Rack closed, and are listed under *Pending* below.

## Files

New:

- `src/MoletaEngine.hpp`: the DSP, header-only, **no Rack includes** (standard library only), so a host test can compile it.
- `src/MoletaExpander.hpp`: the message struct shared by the two modules (plain data, no Rack includes either).
- `src/Moleta.cpp`: the module and its widget.
- `tests/moleta_engine_test.cpp`: host-side tests of the engine (plain `assert`-style checks, no framework, exits non-zero on failure).
- `res/fonts/InterTight-SemiBold.ttf` and `res/fonts/OFL.txt`: copies of the files in `tools/panelgen/fonts/` (the design system says the live readout text ships in `res/fonts/`; `res` is already in `DISTRIBUTABLES`).

Changed:

- `src/widgets.hpp`: a `w72::Readout` widget.
- `src/plugin.cpp`, `src/plugin.hpp`: register `modelMoleta`.
- `plugin.json`: the Moleta entry, with its `manualUrl` on `main` (`docs/modules/moleta.md`) and tags chosen from the valid list.
- `Makefile`: a `test-engine` target that builds and runs the host test with the plain host compiler. It must not touch the plugin's own build or `DISTRIBUTABLES`.
- `.github/workflows/build.yml`: an `engine` job on Linux running that target. Keep the workflow's style and its docs-skip.
- `docs/modules/moleta.md`, `docs/modules/README.md`, `docs/architecture.md`.

Never hand-edit `res/Moleta.svg`, `res/MoletaPlus.svg`, `src/MoletaPanel.hpp` or `src/MoletaPlusPanel.hpp`: they are generated from `panels/*.json`, which are done. If something about a panel seems wrong, stop and report it.

## The engine

A small set of plain types, with these jobs (names and signatures are the implementer's, and should read like the rest of the code):

- **Ratio table.** The 21 ratios of the design, in its order: /16, /8, /7, /6, /5, /4, /3, /2, 2/3, 3/4, ×1, 4/3, 3/2, ×2, ×3, ×4, ×5, ×6, ×7, ×8, ×16. Each is a pair p/q, the number of pulses p per q beats, so ×N is N/1, /N is 1/N, 3/2 is three pulses every two beats. Each has a display name ("×2", "/4", "3/2"; the multiplication sign is U+00D7, UTF-8). Fresh defaults for outputs 1 to 4: ×2, /2, /3, /4.
- **Beat clock.** Produces, per sample, the beat edges and the beat period T in seconds, from either source:
  - *Internal*: BPM, a continuous beat phase; BPM changes take effect at once.
  - *External*: rising edges on CLOCK count; every *k*-th edge is a beat edge (*k* = 1, 4 or 24 from the menu; the first edge after connecting, a RESET or a RUN is edge 0, so a beat edge); T is the time between the last two beat edges (unknown until the second). If no edge arrives for about two periods, it reports inactive and the outputs stop.
  - Pulling the CLOCK cable hands back to the internal clock.
- **Output engine** (one per output, shared by Moleta and Moleta+). From the beat clock and its ratio it decides when a pulse is due:
  - Position within a cycle of q beats advances at 1/T beats per second; a pulse is due each time the position crosses a multiple of q/p beats, and one is due at the cycle restart (position 0). Cycles restart on the beat edge where the beat count since reset is a multiple of q.
  - Dividers therefore count edges and stay exact at any rate. Multipliers and fractions spread their pulses over the last measured period and realign at each cycle restart. A pulse still pending when the restart arrives is dropped. If the clock is slower than predicted the position waits at the end of its cycle instead of wrapping.
  - Outputs are evaluated in index order each sample, so same-sample pulses see each other (this matters for *Follow the output above*).
- **Coin.** Each output keeps a loop of 16 stored random numbers in [0, 1) and a slot pointer (the menu can shorten the loop to 8). On each due pulse, the slot at the pointer is redrawn with probability 1 − REPEAT and kept otherwise (REPEAT fully left redraws every time, fully right never), the pulse passes if the slot's number is below `CHANCE + 0.1 × DENSITY volts` clamped to 0 to 1, and the pointer advances whether or not it passed. RESET sets every pointer back to 0. The random source is injected (a callable returning a uniform float) so tests can fix it; in the module it is Rack's `random::uniform()`.
- **Follow the output above** (a per-output flag, outputs 2 to 4): the output passes only while the last pulse of the output above it actually passed (its final result, after its own coin and follow, so chains cascade). Starts true after a reset.
- **Length.** The knob's lowest value is a 1 ms trigger. Above it, a gate from 5% to 95% of that output's own period, which is T × q / p at the time of the pulse. Gate lengths never reach 100%, so back-to-back pulses always retrigger.
- **The message.** What a consumer needs per sample: whether the module is running and the clock active, a beat edge this sample, a reset this sample, T, REPEAT, DENSITY and the coin loop length. The expander runs the same output engine on this, so it needs nothing else from the base.

## The module (#51)

Match `Ghisa.cpp`'s conventions throughout: new inputs and outputs at the end of the enums (patches store cables by index), `onReset`, `dataToJson`/`dataFromJson`, menus with `createIndexPtrSubmenuItem`, the panel header namespace (`MoletaPanel::`), flat buttons through `w72::createFlatButton`, `DarkPJ301MPort`, `ScrewBlack`.

Controls, from `panels/Moleta.json`:

- **Params:** RUN (latching button, on by default), RESET (momentary button), BPM, REPEAT, RATIO 1 to 4, CHANCE 1 to 4, LENGTH 1 to 4. A fresh Moleta is a plain clock: CHANCE 100%, REPEAT fully left, LENGTH 50% (so a gate of half the period, between the trigger at 0 and the 95% top), 120 BPM. BPM range 30 to 300, shown with one decimal.
- **Inputs:** RUN, CLOCK, RESET, DENSITY (read the first channel).
- **Outputs:** 1 to 4, BEAT, RESET, CLICK.
- **RUN:** the button and a trigger on the input both toggle it. Stopped: the outputs are silent and the internal clock is frozen. Turning RUN on restarts the cycles and the coin pointers like RESET does, without firing the RESET output.
- **RESET:** the button or a trigger on the input restarts every output's cycle and the coin pointers, restarts the bar count, and fires a 1 ms trigger on RESET.
- **BEAT:** a 1 ms trigger on every beat, 10 V, no coin.
- **CLICK:** an audible metronome click on every beat, a short decaying burst, with a higher and louder accent on beat 1 of the bar. Generated only while the output is patched.
- **Pulses** are 10 V.
- **Ratio params** are stepped (21 steps). `dataToJson` saves each output's ratio itself (as p and q), not the step, and `dataFromJson` sets the knob to the matching step (the nearest if the saved ratio is not in the list), so the list can grow without changing saved patches. Rack loads params before `dataFromJson`, so the saved ratio wins.
- **Saved with the patch:** the ratios, the coin loops (stored numbers and pointers per output), and the menu choices. Unknown or missing values load as the defaults.
- **Right-click menu** (the design's table): *Clock input* (1, 4 or 24 pulses per beat, default 1), *Beats per bar* (default 4; a sensible short list, say 2 to 8, plus 12 if cheap), *Coin loop* (8 or 16, default 16), *Follow the output above* (a submenu or three toggles for outputs 2 to 4, off by default).
- **Param display:** RATIO shows its name, CHANCE a percentage, LENGTH shows "Trigger" at the bottom and the percentage of the period above it, BPM one decimal.
- **Readout:** one `w72::Readout` across the well. The BPM cell shows the set tempo, or the measured tempo while CLOCK is patched. **The cell's name stays "BPM"**: the label is baked into the panel, and nothing paints over it. Cells 1 to 4 show each output's ratio name. The browser shows the header's sample values.
- **`w72::Readout`** in `widgets.hpp`: like `FlatBars`, a transparent widget on the well's rectangle, given the cell centres and sizes from the panel header. It draws on the light layer in Inter Tight SemiBold, white ink (`w72::INK`), centred on each cell's baseline point. A virtual hook returns each cell's string. Load the font through the window API the way other plugins do (`APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/InterTight-SemiBold.ttf"))`) and handle a missing font without crashing. Moleta+ uses the same widget.
- **Expander side:** while the module to the right is `modelMoletaPlus` (declared with the other models in `plugin.hpp` only when #52 lands; until then compare against a model pointer that is null, or guard it so #51 compiles on its own), push the message into that module's `leftExpander.producerMessage` each sample and request a flip. While attached, delay **every** base output (1 to 4, BEAT, RESET, CLICK) by one sample, so all eight outputs line up (Rack passes the message one sample late). With no expander, no delay. #51 includes this code and tests the delay and the message in the engine test, without a Moleta+ module.

Every write path stays on the audio thread's own state: there is no main-thread work in these modules.

## The expander (#52)

- `src/MoletaPlus.cpp`, from `panels/MoletaPlus.json`: RATIO 5 to 8, CHANCE 5 to 8, LENGTH 5 to 8, outputs 5 to 8, one readout of the four ratio names. Register `modelMoletaPlus`, add the `plugin.json` entry (display name "Moleta +", slug `MoletaPlus`, `manualUrl` on `main`).
- Allocates both buffers for its message in the constructor (`leftExpander.producerMessage` and `consumerMessage`), frees them in the destructor, and each sample checks that `leftExpander.module` is a `modelMoleta` before reading the consumer message. With no base on its left, the outputs are 0 V.
- Runs the same output engine for outputs 5 to 8 from the message, with its own coin loops, restarted by the message's reset. REPEAT, DENSITY and the loop length come from the base. *Follow the output above* is offered on outputs 6 to 8 only; output 5 starts a fresh chain.
- Saves its own ratios, coin loops and menu choice like the base.
- Manual page `docs/modules/moletaplus.md`, a row in `docs/modules/README.md`, the expander section of `moleta.md` updated to say it ships, and architecture decision rows.

## Documentation

- `docs/modules/moleta.md` becomes the manual: remove the "Designed, not built" note, set **Since** to 2.8.0, point the panel image at `moleta.png` (the Rack render, made later; the mock stays in the repo for now), document the saved state in a table as `ghisa.md` does, and fix whatever the build made different from the design. Keep the open question about the Clocker features: it needs a patch scan this plan does not do.
- `docs/architecture.md`: decision rows dated 2026-11-05 for what this plan settles (the output engine and the p/q ratios, RUN restarting the cycles, LENGTH's trigger step, REPEAT redrawing with probability 1 − REPEAT, follow-above using the final pass state, the message and the one-sample delay on all base outputs, the readout keeping the BPM name, the host-side engine test), and a line in the CI section for the new job.
- All files UTF-8 with LF. After editing any file with non-ASCII characters (× appears in ratio names), check it with `iconv -f UTF-8 -t UTF-8`: a stray Latin-1 byte once made two docs unreadable to the editing tools.

## Verification

1. **Host tests** (`make test-engine`), in the engine test:
   - Ratios: every one of the 21 gives the right number of pulses over its cycle, at the right sample positions within one sample, at 48 kHz and a few BPMs.
   - Dividers stay exact; ×N and p/q realign at each cycle restart; a pulse pending at the restart is dropped.
   - A moving clock (the beat period varying by tens of percent): pulse counts per cycle stay exact, with no accumulated drift.
   - A clock that stops: outputs stop after about two missed periods.
   - Coin: CHANCE 0 and 100%; REPEAT fully right repeats a pattern of the loop's length; lowering CHANCE on a locked pattern only removes pulses (the passing set shrinks to a subset); loops of 8 and 16; RESET restarts the pointers.
   - Length: the trigger is 1 ms; a gate is the right fraction of the output's own period; it never reaches 100%.
   - Follow the output above, including a chain and same-sample pulses.
   - The message round trip: an output engine fed through the message with one sample of latency, against the base delayed by one sample, gives identical output.
2. **Build** from a MinGW64 shell (`make -j4`, with `RACK_DIR` on the make line): no new warnings with the project's flags (`-Wall -Wextra`).
3. **CI:** the Linux, macOS and panels jobs still build.

## Pending after this plan (the owner's Rack must be closed; do not start Rack)

- `docs/modules/moleta.png` and `moletaplus.png` from `Rack -t 2`, as `docs/modules/README.md` describes.
- Live checks in a throwaway Rack: steady tempo; a modulated LFO square into CLOCK with 3/2 and ×3 stretching and not drifting; REPEAT locking a phrase; the loops and ratios surviving a save and reload; RUN and RESET by button and by input; CLICK audible with the accent; the readout showing the measured tempo while following; and, with #52, eight outputs aligned across the expander, the base's outputs delayed only while it is attached, and the expander silent when the base is removed.

## Commits

On `feat/moleta`, Conventional Commits, explicit paths, LF:

1. `docs: fix a stray non-UTF-8 byte in the Moleta default ratios` (already in place, shared with the Ghisa branch; the two apply cleanly in either order)
2. `docs(moleta): plan Moleta and Moleta+` (this file)
3. `feat(moleta): the clock, ratio and coin engine, with host tests` (engine, message header, tests, `Makefile` target, CI job)
4. `feat(moleta): readout widget and the Inter Tight font` (`widgets.hpp`, `res/fonts/`)
5. `feat(moleta): the Moleta module` (`Moleta.cpp`, registration, `plugin.json`)
6. `docs(moleta): the Moleta manual` (manual, README row, architecture rows, CI line)

#51's pull request is titled `feat(moleta): probability master clock` and closes #51; #52's is `feat(moleta): Moleta+ expander` and closes #52. Merging is the owner's call.
