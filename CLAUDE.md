# W72 — VCV Rack plugin

W72 is a VCV Rack plugin (slug `W72`) with modules named in Milanese dialect. The first module is a bridge: a TCP server inside Rack that lets Claude read and patch a running Rack, paired with a Go MCP server in `mcp/`.

Read these before starting any task:

- [docs/concept.md](docs/concept.md): theme, naming grammar, glossary, design principles.
- [docs/architecture.md](docs/architecture.md): architecture, sync procedure, milestones, decisions.
- [docs/design-system.md](docs/design-system.md): the panel style, grid, colours, type and parts. Read it before touching a panel.

The repository is meant to be public. Keep machine paths, personal notes and credentials out of committed files; machine-specific notes belong in a gitignored `CLAUDE.local.md`.

## Layout

The plugin lives at the repo root (`plugin.json`, `Makefile`, `src/`, `res/`) so the VCV Library can build it. `mcp/` (Go), `protocol/`, `panels/` and `tools/panelgen/` (Go) sit beside it and stay out of the plugin package (`DISTRIBUTABLES`).

Panels are generated: edit `panels/<Slug>.json` and run `make panels`, which rewrites `res/<Slug>.svg` and `src/<Slug>Panel.hpp`. Never edit either by hand.

## Rules

- **Slugs are permanent.** A module slug is written into every patch that uses it. Plain ASCII only (Fiòca → `Fioca`). Never rename a shipped slug.
- **No "VCV"** in any name, slug or binary name.
- **The bridge module is called Brumista.** Its slug is `Brumista`. The MCP server takes the same name, so its tools are `mcp__brumista__*`.
- **Port 7200** is the bridge's default. limen (forsitan modulare) uses 7000 and may be running in the same patch: never take its port, and never talk to it as if it were W72.
- **`protocol/commands.json` is the source of truth** for commands. The C++ dispatch is handwritten; a test checks it against the file. The Go server generates its tool list from it. A change to one side without the others is incomplete.
- **Main thread for writes.** Anything that touches the scene or the engine's topology runs through the main-thread queue. Every write pushes a Rack history action so Ctrl+Z works.
- **Never crash Rack.** Validate ports against what the panel exposes, and turn exceptions into error replies.
- **Every new cable gets a colour**: the requested one, or `RackWidget::getNextCableColor()`.
- **Licence:** GPL-3.0-or-later. GPL-3 code from limen, limen-tools or Stoermelder may be reused with attribution in the file header.

## Before writing code: check rackmaster

If the rackmaster plugin is available in the session, use it first:

- Look up existing modules (name collisions, prior art, gaps) in its module index.
- Read the source of open-source modules that already solve the problem. Stoermelder PackOne is the reference for presets and module state.
- Read the errata and technique cards before designing a module.

## Where work runs

- **Cloud threads:** design, docs, the Go MCP server, and C++ that a compile check can verify.
- **Local threads only:** the Windows build (`make install` with MSYS2 and the Rack SDK) and any test against a running Rack. A cloud thread must not claim a Rack test passed.

Close Rack before `make install` on Windows: a loaded plugin `.dll` is locked.

## Git workflow

- **Every change goes through a pull request.** Work on a branch, never directly on `main`; a thread's work ends in a PR, and its review is where it resolves. Merging is the owner's call.
- **Conventional Commits** for commit messages and PR titles: `feat:`, `fix:`, `docs:`, `refactor:`, `test:`, `build:`, `ci:`, `chore:`, with an optional scope (`feat(bridge): add cable colours`, `fix(mcp): …`). A breaking protocol change carries `!` and a `BREAKING CHANGE:` footer.
- Keep commits focused; stage explicit paths, not `git add -A`.
- Files use LF line endings (`.gitattributes` enforces it). On Windows, check that tools didn't write CRLF.

## Versioning

- **Semantic Versioning.** One version for the plugin, the MCP binary and the protocol; they move together.
- Rack requires the plugin's major version to match Rack's (2.x for Rack 2), so W72 versions are `2.MINOR.PATCH`: a new command or module is a minor bump, a fix is a patch. A breaking protocol change is called out in the release notes and the `hello` handshake, since the major version is pinned.
- Version bumps (`plugin.json`, binary, protocol version) go in their own `chore(release): X.Y.Z` commit.

## CI

Lightweight, on GitHub Actions, once there is code to check: build and test the Go server, run the `commands.json` check, and compile the plugin on Linux against the Rack SDK. CI never replaces a local test against a running Rack.
