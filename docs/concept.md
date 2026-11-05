# W72 — Concept and Foundations

2026-11-01

## Why W72 exists

W72 is a VCV Rack plugin by WinterOf72. Its modules are meant to make what's already in the rack go further, not to add one more thing to the pile.

- **Learn to use what you have better.** With thousands of modules available, the question is rarely which module to get. The question is what the ones you have can still do. W72 modules should open new roles for the patch around them.
- **Modular is the source code of sound design.** Records by FSOL, Boards of Canada, Aphex Twin and Autechre raise the question of how a sound was made. A patch answers it the way reading code answers how a program works. W72 modules should be readable that way: a mechanism you can understand, not a black box.
- **Modular is cooking.** The same ingredient does different things in different dishes. A pinch of sugar takes the edge off a tomato sauce. A mixer mixes signals, not only voices. A VCA multiplies. W72 modules should be ingredients like that: a small, clear job that changes meaning with where you put it.
- **Modular is a practice.** It's zen-like and never finished, like bonsai, kanji or a mandala, and always more than the sum of its parts. A W72 module is something to return to, not to master once.

The plugin starts with a studio tool: the bridge that lets Claude patch a running Rack. Sound modules come later, under the same principles.

## The place: a Milanese winter

The plugin's world is a winter in the Po Valley, around Milan. The modules are named in Milanese dialect.

- **A winter, but not a Nordic one.** "WinterOf72" suggests snow and frost, and a plain winter vocabulary (rime, floe, sleet) reads as Scandinavian. The Po Valley winter is the real one behind the name: fog that sits on the plain for days, frost that grows on branches, low light on wet stone and canals.
- **A place, not a list of words.** Ambient and drone patches are places to inhabit, not sequences of events. The plugin works the same way: the names describe one landscape and one city, so the modules belong together the way things in a place do.
- **Dialect, as a wink at Latin.** gosub, a fellow Italian, named the forsitan modulare modules in Latin (limen, scando, alea). W72 makes the same choice in a different register. Latin is the scholar's Italian; Milanese is the street's.
- **The grin test.** A name should make sense to someone who knows the word, and make them smile when they hear what the module does. For everyone else it's simply an unusual name for a good module, which is how most plugin names work anyway.

## Naming grammar

Each kind of module takes its name from one part of Milanese life, so a name says what kind of thing a module is before anyone reads the manual.

| Family | Names come from | For modules that | Examples |
| --- | --- | --- | --- |
| Trades | The city's jobs and figures | Act on the patch: build, route, direct, carry | Magutt, Ghisa, Brumista, Moleta |
| Weather and light | The Po Valley winter | Make or shape sound: space, texture, motion | Scighera, Gibigiana, Galaverna, Fiòca, Merla, Giazz |
| Kitchen | Milanese winter food | Colour and weight: saturation, density, rests | Cassoeula, Michetta |

Rules for names and slugs:

- **The plugin slug and name are `W72`.** The slug was free on 2026-11-01: no VCV Library search result, and no manifest in the VCVRack/library repo.
- **A module slug is permanent** once a patch uses it: it's written into every `.vcv` file. Display names can change later; slugs can't.
- **Slugs are plain ASCII**: accents are dropped (Fiòca → `Fioca`), but the display name keeps them.
- **No "VCV" in any name.** VCV's licence reserves it.
- **A name is reserved, not spent, until a module ships.** Candidates can wait in the glossary without commitment.

## Glossary

Two names are shipped modules in the VCV Library (Brumista, Ghisa), three are approved for modules to come (Moleta, in design, then Scighera and Gibigiana), and the rest are candidates. Spellings follow common use; classical Milanese orthography varies.

| Name | Meaning | Module role | Status |
| --- | --- | --- | --- |
| Brumista | The old cab driver, from the brougham carriage | The bridge: carries Claude's commands into a running Rack, where it's told ([manual](modules/brumista.md)) | Shipped ([Library](https://library.vcvrack.com/W72/Brumista)) |
| Ghisa | The Milanese traffic warden, nicknamed after the old helmet | A shift register whose taps are a control: chords and a bass from the melody's recent past ([manual](modules/ghisa.md)) | Shipped ([Library](https://library.vcvrack.com/W72/Ghisa)) |
| Moleta | The itinerant knife grinder (also Molèta, Molètta), pedalling a steady wheel that throws sparks at random | A master clock whose outputs each have a ratio, a chance and a length ([design](modules/moleta.md)) | Approved, in design |
| Scighera | The thick Po Valley fog | Diffusion, reverb, smeared noise | Approved |
| Gibigiana | The patch of reflected sunlight that dances on a wall, off water or glass | Shimmer, moving reflection or delay | Approved |
| Magutt | The bricklayer or labourer who builds what the architect drew | Passed over for the bridge: Claude builds the patch you design | Candidate |
| Tram | The city's trams, carrying people between stops on fixed rails | Passed over for the bridge: carries commands on a fixed protocol | Candidate |
| Naviglio | The canals that carried the Duomo's marble into the city | Passed over for the bridge: a channel into the rack | Candidate |
| Galaverna | Frost that grows on branches from freezing fog | Freezer, accumulating texture | Candidate |
| Fiòca | Snow | Granular, falling particles | Candidate |
| Merla | *I giorni della merla*, the last three days of January, the coldest by folklore | Very slow three-stage modulator | Candidate |
| Giazz | Ice | Sample-and-hold, freeze | Candidate |
| Cassoeula | The pork-and-cabbage winter stew | Thick saturation | Candidate |
| Michetta | The hollow Milanese bread roll | Mostly air: a sparse gate or rest generator | Candidate |

## Design principles

These are the test a module has to pass, more than a feature list.

1. **Restraint.** A module should make the smaller move well: gentle compression, low probabilities, a barely-there FM that you feel more than hear. Its range of useful settings starts at subtle.
2. **A fixed point.** Drift and chaos read as expression only against something stable. Modules should make it easy to anchor one element: a held value, a ceiling, a pedal tone.
3. **Multi-role, like an ingredient.** A small, clear job that works on audio and CV alike, and does something different depending on where it sits in the patch. No module should care whether its input is a voice or a modulation.
4. **Readable mechanism.** You should be able to understand what a module does from its panel and a short manual, and explain why it sounds the way it does. No hidden state that the patch can't see.
5. **Places, not events.** Sound modules should support patches you inhabit for minutes: slow change, long memory, evolution over sequence.
6. **Set by ear.** Controls are tuned for listening, not for numbers. Ranges and tapers should make the musical region easy to find with a knob.

## The first module: the bridge

The first W72 module, Brumista, lets Claude read and patch a running Rack, so a patch can be designed in conversation and built while you listen.

It grows out of limen, gosub's module in forsitan modulare, which proved the idea. Using it every day showed what a studio needs on top:

- **Patches that read like yours.** Cables in a consistent colour scheme (for example blue clock, yellow pitch, green modulation, red audio), not invisible ones. In limen, cables added through its protocol got no colour at all until forsitan 2.17.1 fixed it ([forsitan-modulare#24](https://github.com/gosub/forsitan-modulare/issues/24)).
- **Seeing what the panel sees.** Module state that lives outside parameters (a quantizer's scale mask, a mixer's gain trims) is visible today only by saving the patch and reading the file.
- **Changes you can take back.** Every change Claude makes should be undoable with Ctrl+Z, like your own.
- **Mistakes that don't crash Rack.** A cable to a port with no panel jack should be refused, not take Rack down.
- **Presets and selections as building blocks.** Dropping a saved preset or `.vcvs` selection in directly, so settled voices get reused.

No extra runtime should be needed to use it: the module plus one downloaded binary, with nothing else to install. The architecture, protocol and build are in [architecture.md](architecture.md).

## Open questions

- [ ] Will W72 hold sound modules as well as studio tools, or will tools get their own plugin later?
- [x] What is the panel style: what does a Milanese winter look like on a W72 panel? Metropolitana, after the Milan Metro's signage: see [design-system.md](design-system.md).
- [ ] Which sound module comes second: Ghisa, Scighera or Gibigiana?
