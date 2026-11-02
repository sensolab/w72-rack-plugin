# W72

Modules for [VCV Rack](https://vcvrack.com/) by WinterOf72, named in Milanese dialect after a Po Valley winter.

The first module is a bridge that lets Claude read and patch a running Rack, together with a small MCP server. Sound modules (Ghisa, Scighera, Gibigiana) come later.

The first release, 2.1.0, ships Brumista; each module has a page in [docs/modules](docs/modules/README.md). The project's foundations are in [docs/concept.md](docs/concept.md) (theme, naming, design principles) and [docs/architecture.md](docs/architecture.md) (architecture, milestones, build).

## The MCP server

`brumista-mcp` connects Claude Code to a Brumista module in a running Rack, on `127.0.0.1:7200`. Build it with Go from `mcp/` (`go build -o bin/brumista-mcp .`), then register it:

```
claude mcp add brumista -- /path/to/brumista-mcp
```

`-host` and `-port` point it elsewhere. Release builds will be on GitHub Releases.

## Licence

GPL-3.0-or-later. See [LICENSE](LICENSE).
