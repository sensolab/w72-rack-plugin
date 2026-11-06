# W72

Modules for [VCV Rack](https://vcvrack.com/) by WinterOf72, named in Milanese dialect after a Po Valley winter.

| Module | What it does |
| --- | --- |
| [Brumista](docs/modules/brumista.md) | A bridge that lets any MCP-capable agent read and patch a running Rack, paired with an MCP server. |
| [Ghisa](docs/modules/ghisa.md) | A shift register whose taps are a control: a drawn shape, slid through the register by CV, read as a chord and a bass. |
| [Moleta](docs/modules/moleta.md) | A master clock whose four outputs each have a ratio (fractions included), a chance and a length; a REPEAT knob loops the coin flips into a phrase. |
| [Moleta +](docs/modules/moletaplus.md) | Its expander: place it directly to the right of a Moleta for outputs 5 to 8. |
| [Gibigiana](docs/modules/gibigiana.md) | A clocked echo lane: four taps on the clock grid, each with a chance per hit; the coin rides with the echo, so a note and its tail come back whole or not at all. |

Scighera is planned. Each module has a page in [docs/modules](docs/modules/README.md). The project's foundations are in [docs/concept.md](docs/concept.md) (theme, naming, design principles) and [docs/architecture.md](docs/architecture.md) (architecture, milestones, build), and the panel style is in [docs/design-system.md](docs/design-system.md).

## Install

W72 is in the [VCV Library](https://vcvrack.com/plugins) as "W72 by WinterOf72", for Rack 2. Subscribe to it there and it appears in Rack's Plugin Manager (Library menu, then Log in and Update all). The Library lists a new version some time after each release here. To build it yourself instead, see below.

## Build

You need the [Rack SDK](https://vcvrack.com/manual/Building) matching your Rack version (2.6.6 is what CI uses) and the usual Rack plugin toolchain.

```
export RACK_DIR=/path/to/Rack-SDK
make          # builds the plugin
make install  # copies it into Rack's user plugins folder
```

`make test-engine` runs the host tests of Moleta's and Gibigiana's engines and Brumista's engine watch with the plain host compiler; it needs no Rack SDK.

Panels are generated. Edit `panels/<Slug>.json` and run `make panels` (needs Go), which rewrites `res/<Slug>.svg` and `src/<Slug>Panel.hpp`. Do not edit those by hand.

## The MCP server

`brumista-mcp` connects any MCP-capable agent to a Brumista module in a running Rack, on `127.0.0.1:7200`. It is not part of the Library package: download the binary for your platform from [GitHub Releases](https://github.com/sensolab/w72-rack-plugin/releases) (`brumista-mcp-<version>-<os>-<arch>`, with `SHA256SUMS`), or build it with Go from `mcp/` (`go build -o bin/brumista-mcp .`). Use the same version as the plugin. Then register it with your agent as a stdio MCP server; for Claude Code:

```
claude mcp add brumista -- /path/to/brumista-mcp
```

`-host` and `-port` point it elsewhere, for instance at a port set in Brumista's right-click menu. Brumista listens on the loopback address only and has no authentication: any program on the same machine can use it to edit and save patches. See [Brumista](docs/modules/brumista.md#security).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## Credits

- [limen](https://github.com/gosub/forsitan-modulare) by gosub (forsitan modulare, GPL-3.0-or-later) is the bridge module that Brumista follows. Brumista speaks limen's command protocol, and its socket loop follows the shape of limen's. See [Relation to limen](docs/modules/brumista.md#relation-to-limen).
- Ghisa's root bass scores intervals as the Harmonic mode of bss in [cvly-modules](https://github.com/Lyqst/cvly-modules) by Lyqst (GPL-3.0) does.
- Panel text is set in [Inter Tight](https://github.com/rsms/inter-tight) (SIL Open Font License 1.1, in [tools/panelgen/fonts](tools/panelgen/fonts)). The SemiBold weight also ships in [res/fonts](res/fonts), with its licence, for the readouts of Moleta and Gibigiana.

## Licence

GPL-3.0-or-later. See [LICENSE](LICENSE).
