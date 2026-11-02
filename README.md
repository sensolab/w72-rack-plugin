# W72

Modules for [VCV Rack](https://vcvrack.com/) by WinterOf72, named in Milanese dialect after a Po Valley winter.

| Module | What it does |
| --- | --- |
| [Brumista](docs/modules/brumista.md) | A bridge that lets Claude read and patch a running Rack, paired with an MCP server. |
| [Ghisa](docs/modules/ghisa.md) | A shift register whose taps are a control: a drawn shape, slid through the register by CV, read as a chord and a bass. |

Scighera and Gibigiana are planned. Each module has a page in [docs/modules](docs/modules/README.md). The project's foundations are in [docs/concept.md](docs/concept.md) (theme, naming, design principles) and [docs/architecture.md](docs/architecture.md) (architecture, milestones, build), and the panel style is in [docs/design-system.md](docs/design-system.md).

## Install

The plugin is built for Rack 2. Until it is in the VCV Library, build it from source (below) or take a package from [GitHub Releases](https://github.com/sensolab/w72-rack-plugin/releases) when one is attached.

## Build

You need the [Rack SDK](https://vcvrack.com/manual/Building) matching your Rack version (2.6.6 is what CI uses) and the usual Rack plugin toolchain.

```
export RACK_DIR=/path/to/Rack-SDK
make          # builds the plugin
make install  # copies it into Rack's user plugins folder
```

Panels are generated. Edit `panels/<Slug>.json` and run `make panels` (needs Go), which rewrites `res/<Slug>.svg` and `src/<Slug>Panel.hpp`. Do not edit those by hand.

## The MCP server

`brumista-mcp` connects Claude Code to a Brumista module in a running Rack, on `127.0.0.1:7200`. Get it from GitHub Releases, or build it with Go from `mcp/` (`go build -o bin/brumista-mcp .`), then register it:

```
claude mcp add brumista -- /path/to/brumista-mcp
```

`-host` and `-port` point it elsewhere. Brumista listens on the loopback address only and has no authentication: any program on the same machine can use it to edit and save patches. See [Brumista](docs/modules/brumista.md#security).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## Credits

- [limen](https://github.com/gosub/forsitan-modulare) by gosub (forsitan modulare, GPL-3.0-or-later) is the bridge module that Brumista follows. Brumista speaks limen's command protocol, and its socket loop follows the shape of limen's. See [Relation to limen](docs/modules/brumista.md#relation-to-limen).
- Ghisa's root bass scores intervals as the Harmonic mode of bss in [cvly-modules](https://github.com/Lyqst/cvly-modules) by Lyqst (GPL-3.0) does.
- Panel text is set in [Inter Tight](https://github.com/rsms/inter-tight) (SIL Open Font License 1.1, in [tools/panelgen/fonts](tools/panelgen/fonts)).

## Licence

GPL-3.0-or-later. See [LICENSE](LICENSE).
