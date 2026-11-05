# W72 modules

One page per module: what it does, its panel, and how to use it. The design behind the modules is in [concept.md](../concept.md) and [architecture.md](../architecture.md).

| Module | Family | HP | What it does | Since |
| --- | --- | --- | --- | --- |
| [Brumista](brumista.md) | Trades | 3 | Bridge that lets Claude read and patch a running Rack | 2.1.0 |
| [Ghisa](ghisa.md) | Trades | 8 | Shift register whose taps are a control: a drawn shape, slid through the register by CV, read as a chord and a bass | 2.3.0 |

Each module's `manualUrl` in `plugin.json` points to its page here on `main` (`https://github.com/sensolab/w72-rack-plugin/blob/main/docs/modules/<slug-lowercase>.md`), so Rack's right-click Manual opens it. A new module ships with its page and that link.

The panel images are Rack's own renders, not the panel SVGs (GitHub can't draw Rack's knobs, jacks and lights). Regenerate them after a panel change with `Rack -u <empty folder> -t 2`, with only the W72 `.vcvplugin` in that folder's `plugins-<os>-<arch>/`; Rack writes `screenshots/W72/<Slug>.png`, which becomes `<slug-lowercase>.png` here.

Designed, not built yet:

| Module | Family | HP | What it does |
| --- | --- | --- | --- |
| [Moleta](moleta.md) | Trades | 18 | Master clock whose four outputs each have a ratio (fractions included), a chance and a length; the coin flips can loop |

Scighera and Gibigiana are approved names for modules still to come; the glossary in [concept.md](../concept.md#glossary) has the candidates.
