# W72 — VCV Rack plugin

W72 is a VCV Rack plugin (slug `W72`) with modules named in Milanese dialect. Seven modules: Brumista, a bridge (a TCP server inside Rack that lets Claude read and patch a running Rack, paired with a Go MCP server in `mcp/`); Ghisa, a shift register whose taps are a control; Moleta, a probability master clock; its expander Moleta +, four more outputs each, up to four in a chain; Gibigiana, clocked echo lanes with a chance per hit, one lane of its own; its lane expander Gibigiana +, one more lane each, up to 8 lanes; and Barbapedana, a 16-step sequencer crossed with a Turing-style random loop.

Read these before starting any task:

- [docs/concept.md](docs/concept.md): theme, naming grammar, glossary, design principles.
- [docs/architecture.md](docs/architecture.md): architecture, sync procedure, milestones, decisions.
- [docs/design-system.md](docs/design-system.md): the panel style, grid, colours, type and parts. Read it before touching a panel.

The repository is meant to be public. Keep machine paths, personal notes and credentials out of committed files; machine-specific notes belong in a gitignored `CLAUDE.local.md`.

## Layout

The plugin lives at the repo root (`plugin.json`, `Makefile`, `src/`, `res/`) so the VCV Library can build it. `mcp/` (Go), `protocol/`, `panels/`, `tests/` (host tests of Moleta's, Gibigiana's and Barbapedana's engines and Brumista's engine watch) and `tools/panelgen/` (Go) sit beside it and stay out of the plugin package (`DISTRIBUTABLES`). `res/` holds the generated panels and the one Inter Tight weight the readouts draw (`res/fonts/`). Moleta's DSP is a header with no Rack includes (`src/MoletaEngine.hpp`), shared with Moleta +, so it can be tested without Rack; Gibigiana's (`src/GibigianaEngine.hpp`) is the same, and reuses Moleta's coin loop; Barbapedana's (`src/BarbapedanaEngine.hpp`) too.

Panels are generated: edit `panels/<Slug>.json` and run `make panels`, which rewrites `res/<Slug>.svg` and `src/<Slug>Panel.hpp`. Never edit either by hand.

## Rules

- **Slugs are permanent.** A module slug is written into every patch that uses it. Plain ASCII only (Fiòca → `Fioca`). Never rename a shipped slug.
- **No "VCV"** in any name, slug or binary name.
- **The bridge module is called Brumista.** Its slug is `Brumista`. The MCP server takes the same name, so its tools are `mcp__brumista__*`.
- **Port 7200** is the bridge's default. limen (forsitan modulare) uses 7000 and may be running in the same patch: never take its port, and never talk to it as if it were W72.
- **`protocol/commands.json` is the source of truth** for commands. The C++ dispatch is handwritten; a test checks it against the file. The Go server generates its tool list from it. A change to one side without the others is incomplete.
- **Main thread for writes.** Anything that touches the scene or the engine's topology runs through the main-thread queue. Every write pushes a Rack history action so Ctrl+Z works.
- **Never crash Rack.** Validate ports against what the panel exposes, and turn exceptions into error replies.
- **Every module has a manual page**: `docs/modules/<slug-lowercase>.md`, listed in `docs/modules/README.md`, with the module's `manualUrl` in `plugin.json` pointing to it on `main`. Its panel image is Rack's own render (`docs/modules/<slug-lowercase>.png`, made with `Rack -t 2` as `docs/modules/README.md` describes), never the panel SVG, and is regenerated whenever the panel changes.
- **Every new cable gets a colour**: the requested one, or `RackWidget::getNextCableColor()`.
- **Licence:** GPL-3.0-or-later. GPL-3 code from limen, limen-tools or Stoermelder may be reused with attribution in the file header.

## Before writing code: check rackmaster

If the rackmaster plugin is available in the session, use it first:

- Look up existing modules (name collisions, prior art, gaps) in its module index.
- Read the source of open-source modules that already solve the problem. Stoermelder PackOne is the reference for presets and module state.
- Read the errata and technique cards before designing a module.

## Where work runs

- **Cloud threads:** design, docs, the Go MCP server, C++ that a compile check can verify, and `make test-engine` (Moleta's, Gibigiana's and Barbapedana's engines and Brumista's engine watch, host compiler only).
- **Local threads only:** the Windows build (`make install` with MSYS2 and the Rack SDK) and any test against a running Rack. A cloud thread must not claim a Rack test passed.

Close Rack before `make install` on Windows: a loaded plugin `.dll` is locked.

## Git workflow

- **Every change goes through a pull request.** Work on a branch, never directly on `main`; a thread's work ends in a PR, and its review is where it resolves. Merging is the owner's call.
- **Conventional Commits** for commit messages and PR titles: `feat:`, `fix:`, `docs:`, `refactor:`, `test:`, `build:`, `ci:`, `chore:`, with an optional scope (`feat(bridge): add cable colours`, `fix(mcp): …`). A breaking protocol change carries `!` and a `BREAKING CHANGE:` footer.
- Keep commits focused; stage explicit paths, not `git add -A`.
- Files use LF line endings (`.gitattributes` enforces it). On Windows, check that tools didn't write CRLF.
- Keep links to Claude sessions out of PR and issue bodies and commit messages: they are public. A plain `Generated with Claude Code` footer or `Co-Authored-By` trailer is fine.
- An implementation plan is a working note, not repo content: keep it outside the repo and out of commits.

## Versioning

- **Semantic Versioning.** One version for the plugin, the MCP binary and the protocol; they move together.
- Rack requires the plugin's major version to match Rack's (2.x for Rack 2), so W72 versions are `2.MINOR.PATCH`: a new command or module is a minor bump, a fix is a patch. A breaking protocol change is called out in the release notes and the `hello` handshake, since the major version is pinned.
- Version bumps (`plugin.json`, binary, protocol version) go in their own `chore(release): X.Y.Z` commit.
- A release: merge the `chore(release): X.Y.Z` PR once CI is green, tag `vX.Y.Z` on its merge commit (an annotated tag, message `W72 X.Y.Z`) so the release workflow publishes the binaries, put a short summary above the generated release notes, comment on the Library issue (below), and close the milestone.

## VCV Library

W72 is in the VCV Library (accepted 2026-11-05). After each release, comment the new version and commit hash on the Library submission issue (VCVRack/library#1010); the Library rebuilds from that commit. Brumista's MCP binary is not part of the Library package: it ships on GitHub Releases from the `v2.x.y` tag.

## CI

Lightweight, on GitHub Actions: build and test the Go server, run the `commands.json` check, check the generated panels, run the host tests (Moleta's, Gibigiana's and Barbapedana's engines, Brumista's engine watch), and compile the plugin against the Rack SDK on Linux, macOS x64 and macOS arm64. A change that only touches docs skips this workflow. A tag `vX.Y.Z` runs the release workflow. CI never replaces a local test against a running Rack.
