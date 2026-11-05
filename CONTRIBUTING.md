# Contributing

Issues and pull requests are welcome. For anything larger than a fix, open an issue first so the idea can be talked through before the work is done.

The design lives in [docs/concept.md](docs/concept.md), [docs/architecture.md](docs/architecture.md) and [docs/design-system.md](docs/design-system.md). [CLAUDE.md](CLAUDE.md) lists the project's rules in short form (slugs, ports, threading, versioning); they apply to every contributor.

## Workflow

- Work on a branch and open a pull request against `main`. Nothing is committed to `main` directly.
- Commit messages and PR titles follow [Conventional Commits](https://www.conventionalcommits.org/): `feat:`, `fix:`, `docs:`, `refactor:`, `test:`, `build:`, `ci:`, `chore:`, with an optional scope, as in `fix(mcp): …`.
- Keep commits focused and stage explicit paths.
- Files use LF line endings (`.gitattributes` enforces it).

## Things that go together

- **Panels are generated.** Edit `panels/<Slug>.json`, run `make panels`, commit the regenerated `res/<Slug>.svg` and `src/<Slug>Panel.hpp`. CI fails when they differ from what the generator writes.
- **`protocol/commands.json` is the source of truth for Brumista's commands.** The C++ dispatch in `src/BrumistaCommands.cpp` is checked against it by `tools/check-commands.sh`, and the Go server generates its tools from it. Change all three together.
- **Every module has a manual page** in `docs/modules/`, listed in `docs/modules/README.md`, with Rack's own render of its panel.
- **Moleta's DSP lives in `src/MoletaEngine.hpp`**, which has no Rack includes and is shared with Moleta +. Change it together with `tests/moleta_engine_test.cpp`.
- **Module slugs are permanent**: plain ASCII, never renamed once shipped.

## Checks

```
tools/check-commands.sh                  # commands.json against the C++ dispatch
(cd mcp && go test ./...)                # MCP server
(cd tools/panelgen && go test ./...)     # panel generator and generated files
make test-engine                         # Moleta's engine, host compiler only, no SDK
make                                     # plugin, needs the Rack SDK (RACK_DIR)
```

CI runs the same on every pull request, and compiles the plugin on Linux and on macOS (x64 and arm64). It cannot replace a test against a running Rack: say in the PR what was and was not tried there.

## Licence

By contributing you agree that your work is released under GPL-3.0-or-later, like the rest of the project. Code reused from another GPL-3 project is credited in the file header.
