# Ghisa: duplicate notes and hold loop length

Implementation plan for [#42](https://github.com/sensolab/w72-rack-plugin/issues/42) (a *Duplicate notes* menu item) and [#43](https://github.com/sensolab/w72-rack-plugin/issues/43) (a *Hold loop length* menu item), delivered together in one pull request for milestone 2.8.0. Both are right-click menu items on Ghisa; both default to today's behaviour, so existing patches sound the same.

## Scope

- Changes `src/Ghisa.cpp`, `docs/modules/ghisa.md` and `docs/architecture.md`.
- The panel does not change: no `panels/Ghisa.json` edit, no `make panels`, no new `docs/modules/ghisa.png`.
- No protocol change (`protocol/commands.json` stays as it is). `get_module_data` returns `dataToJson()`, so the new keys show up there without a change.
- No version bump: the `chore(release): 2.8.0` commit comes later, with the rest of the milestone.
- Not in this plan: marking the loop on the REGISTER display, a deepest-tapped-stage loop length, a cap on how high *Octave up* can stack.

## Design

### Hold loop length (#43)

- Menu item **Hold loop length**: 16 stages (default), 12, 8, 6, 4, 3.
- While HOLD is on, each clock feeds stage *N* back into stage 1, where *N* is the loop length, instead of sampling IN. Today it feeds stage 16.
- Stages above *N* keep shifting exactly as they do now. They receive the loop's values, so after at most 16 − *N* clocks of HOLD they hold periodic copies of it: a tap above *N* reads the same note as the tap *N* stages earlier, and every lit button keeps sounding. Until then (the first 16 − *N* clocks) those taps still read the older melody as it shifts out. Releasing HOLD is seamless, because the register is the only state: nothing is indexed modulo *N*.
- The length is read at the clock edge, so a change lands on the next clock like every other setting.
- Saved as `holdLength`, the number of stages (16, 12, 8, 6, 4 or 3). A missing or unknown value loads as 16. `onReset` sets 16.
- The HOLD input's tooltip description says "While high, stage 16 feeds stage 1"; make it say that the last stage of the loop feeds stage 1.

### Duplicate notes (#42)

- Menu item **Duplicate notes**: *Keep* (default, today's behaviour), *Skip*, *Octave up*.
- Where it runs in `latch()`: read the taps, fold them by the chord range, **then remove duplicates in stage order, then sort if Chord order is By pitch**. Deduplicating after the fold catches notes that only become equal once folded.
- Two notes are the same when they differ by 1 mV or less, the tolerance CHANGE already uses. Give it one named constant and use it in both places.
- *Skip*: a channel whose voltage equals a lower-indexed channel is dropped, and CHORD's channel count shrinks.
- *Octave up*: walking the channels in stage order, a channel equal to any earlier one gains +1 V until it matches none (so the second copy goes up one octave, the third two, and a bumped note that lands on another tap keeps climbing). The count stays fixed. It can leave the chord range window and is not capped; the manual says so.
- **BASS reads the raw taps, as it does today**, before the fold and before this step: *Lowest* stays the melody's real low note and *Root* still counts a repeated note once per tap. `bassOf()` currently uses `channels` as the number of taps in `taps[]`. With *Skip* the two numbers differ, so keep a separate `tapCount` for the raw taps and have `bassOf()` use it.
- CHANGE needs no new logic: channels are compared as they leave CHORD, a channel that is new (`c >= channels`) fires, and channels that disappear just stop. With *Skip* the channels above a vanished duplicate renumber, so a voice can re-strike when the channel count moves; *By pitch* order keeps the numbering stable. Document this and recommend *Skip* with *By pitch*.
- Saved as `duplicates`: `keep`, `skip` or `octave`. A missing or unknown value loads as `keep`. `onReset` sets `keep`.

## Code changes in `src/Ghisa.cpp`

1. Constants: a `SAME_VOLTAGE` tolerance (1e-3f) used by the CHANGE comparison, and the table of loop lengths `{16, 12, 8, 6, 4, 3}`. Leave the 1 ms pulse length as it is: that one is a time.
2. A `DuplicateMode` enum (`DUP_KEEP`, `DUP_SKIP`, `DUP_OCTAVE`), a `duplicates` member, and a `holdLength` member holding an index into the table (the menu helper edits an int index; the JSON stores the stage count).
3. A small static helper that removes duplicates from `next[]` for the mode and returns the new count, beside `sortLowToHigh`.
4. `latch()`: keep two counts apart. `n` is the number of lit taps (it becomes the raw tap count), and the chord's channel count can end up smaller. `taps[]` and `tapCount` take the raw count; `chord[]` and `channels` take the deduplicated one. Check every place that used `n` for both.
5. `bassOf()`: use `tapCount` instead of `channels`.
6. `process()`: the HOLD feedback reads `reg[loopLength - 1]`.
7. `onReset()`, `dataToJson()`, `dataFromJson()`: the two new keys, with the defaults above.
8. `appendContextMenu()`: **Duplicate notes** after *Chord order*, and **Hold loop length** last, with choices "16 stages", "12 stages", "8 stages", "6 stages", "4 stages", "3 stages".
9. The HOLD input description (see above).

## Documentation

`docs/modules/ghisa.md`:

- HOLD row in the panel table and the **HOLD** bullet in *How it works*: the loop is the *Hold loop length*, 16 by default, and what the stages above it do.
- A **Duplicate notes** bullet in *How it works*: the order (fold, dedupe, sort), the 1 mV tolerance, BASS reading the raw taps, the CHANGE note for *Skip*, and that *Octave up* can leave the chord range window.
- Two rows in the right-click menu table, and two rows in the saved-state table.
- *A phrase and its echo*: with a loop length of 8, a HOLD gate of eight clocks gives a clean repeat; with 6 or 12, triplet and 3/4 grids.
- *Avoid the collapse*: mention *Duplicate notes* as the cure for a slow theme against a faster clock, next to the existing advice.

`docs/architecture.md`: two decision rows dated with the day they land (loop length and how the tail behaves; duplicate notes order of operations and BASS reading the raw taps), and the open question about HOLD's loop length resolved.

## Verification

There are no C++ unit tests in the repo; CI compiles the plugin. So:

1. **Build** from a MinGW64 shell with `make -j4`: no new warnings.
2. **Live test in a throwaway Rack**, never the owner's patches. A patch can be plain JSON with a Ghisa, a clock source, a quantized or stepped source, a Fundamental Split to read the poly outputs, and a Brumista to read them through `list_ports` and `get_module_data`; the patch's `data` field tests `dataFromJson` directly. Check:
   - A fresh Ghisa is unchanged: the default menu choices give the same CHORD, CHANGE and BASS as 2.7.0.
   - **Loop length 8** with HOLD on and a ramp at IN: the chord repeats every 8 clocks, and after 16 − 8 clocks a tap above stage 8 reads the same note as the tap 8 stages earlier. A HOLD gate of 9 clocks completes one loop. Releasing HOLD does not jump.
   - **Duplicate notes**, with a slow source against a faster clock and the shape 1·3·4·5·6·8: *Keep* shows repeated notes; *Skip* shows fewer channels and none repeated; *Octave up* keeps the channel count and no two channels equal. BASS is the same in all three modes. CHANGE fires for channels that appear and stops for channels that vanish.
   - **Save and reload**: `get_module_data` shows `holdLength` and `duplicates`; a patch with `"holdLength": 5` or `"duplicates": "x"` loads as 16 and `keep`; a patch from 2.7.0 with neither key loads as 16 and `keep`.
3. Rack Free refuses a second instance, so the owner's Rack must be closed first for step 2. Ask; never close it.

## Commits and pull request

On the branch `feat/ghisa-duplicates-hold-loop`, Conventional Commits, explicit paths, LF line endings:

1. `docs(ghisa): plan duplicate notes and hold loop length` (this file).
2. `feat(ghisa): hold loop length (#43)`: the code and its manual and architecture changes.
3. `feat(ghisa): duplicate notes (#42)`: the code and its manual and architecture changes.

One pull request, titled `feat(ghisa): duplicate notes and hold loop length`, whose description closes #42 and #43. Merging is the owner's call.
