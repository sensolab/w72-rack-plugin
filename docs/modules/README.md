# W72 modules

One manual per module: what it does, its panel, and how to start using it. Each manual has a reference page beside it (`<slug>-reference.md`) with the exact ranges, timing, edge cases and saved state. The design behind the modules is in [concept.md](../concept.md) and [architecture.md](../architecture.md).

| Module | Family | HP | What it does | Since |
| --- | --- | --- | --- | --- |
| [Brumista](brumista.md) | Trades | 3 | Lets an AI agent read and patch your running Rack. | 2.1.0 |
| [Ghisa](ghisa.md) | Trades | 8 | Feed Ghisa a melody, and it plays chords made from the notes the melody just played. | 2.3.0 |
| [Moleta](moleta.md) | Trades | 18 | A clock whose four outputs each skip beats by chance, and can lock what they skipped into a phrase. | 2.8.0 |
| [Moleta +](moletaplus.md) | Trades | 11 | Four more outputs for the Moleta on its left, each with its own ratio, chance and length. | 2.8.0 |
| [Gibigiana](gibigiana.md) | Weather and light | 14 | Four echoes on the clock grid, each with its own chance of coming back. | 2.9.0 |

Each module's `manualUrl` in `plugin.json` points to its page here on `main` (`https://github.com/sensolab/w72-rack-plugin/blob/main/docs/modules/<slug-lowercase>.md`), so Rack's right-click Manual opens it. A new module ships with its page and that link.

The panel images are Rack's own renders, not the panel SVGs (GitHub can't draw Rack's knobs, jacks and lights). Regenerate them after a panel change with `Rack -u <empty folder> -t 2`, with only the W72 `.vcvplugin` in that folder's `plugins-<os>-<arch>/`; Rack writes `screenshots/W72/<Slug>.png`, which becomes `<slug-lowercase>.png` here.

Scighera is an approved name for a module still to come; the glossary in [concept.md](../concept.md#glossary) has the candidates.
