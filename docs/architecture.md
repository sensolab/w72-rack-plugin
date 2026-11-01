# W72 — Technical Foundations

2026-11-01

## At a glance

W72's first deliverable is a Rack module and a small MCP binary, built from one repo and one command list.

```mermaid
flowchart LR
    client["Claude Code<br/>MCP client"] -- stdio --> bin["MCP binary (Go)<br/>generated tool list"]
    bin -- "TCP, JSON" --> server
    cmds["commands.json<br/>one source of truth"] -. generates tools .-> bin
    cmds -. tested against .-> server
    subgraph rack["Rack: Brumista, the bridge module"]
        server["Server thread<br/>127.0.0.1, JSON over TCP"] -- "reads and writes" --> queue["Main-thread queue<br/>undo, validation, timeouts"]
        queue -- main thread --> engine["Engine and scene<br/>modules, cables, params"]
    end
```

Every command that touches the engine or the scene, read or write, is queued and run on Rack's main thread, where undo and validation happen. Only commands that read plugin data, which doesn't change while Rack runs, answer straight from the server thread.

## Repository and sync

One GitHub repository, `w72-rack-plugin`, holds the plugin, the MCP binary and the protocol definition, released together under one version number. The plugin sits at the repo root, because the VCV Library builds a plugin from its repository at a given commit and has no setting for a subfolder.

```
w72-rack-plugin/
  plugin.json             the Rack plugin, slug W72
  Makefile
  src/                    C++: the bridge module, later the sound modules
  res/                    panels (SVG), generated
  panels/                 one description per panel
  tools/panelgen/         the panel generator, Go (not in the plugin package)
  mcp/                    the MCP server, Go (not in the plugin package)
  protocol/
    commands.json         every command: name, parameters, description
    protocol.go           embeds commands.json for the Go code
    limen/commands.json   limen's commands, the parity oracle
  docs/
    concept.md
    architecture.md       this document
    design-system.md      the panel style
  CLAUDE.md               what every Claude Code thread reads first
```

The sync procedure keeps the two artifacts from drifting:

1. **One source of truth.** `protocol/commands.json` lists every command. The Go server generates its MCP tool list from it. The C++ dispatch table is handwritten and checked against it by `tools/check-commands.sh` in CI: the same commands, in the same order, on the same thread. `hello` builds its command list from that table, so a live `hello` agrees with the file too. Generating the C++ is a fallback if drift becomes a problem. Adding a command is one edit, and a mismatch fails the test.
2. **One version, semver.** `plugin.json`'s version, the binary's version and the protocol version move together under Semantic Versioning. Rack pins the major version to its own (2.x for Rack 2), so new commands or modules are minor bumps and fixes are patches. A release tags all three.
3. **A version handshake.** The module's `hello` reports its protocol version. The binary refuses a version it doesn't know, or warns, so an old pair says so instead of half working.
4. **Separate distribution.** The plugin goes through the VCV Library; the binary ships as prebuilt files on GitHub Releases, built by GitHub Actions. This is how forsitan modulare does it ([limen-tools](https://github.com/gosub/limen-tools/releases)). The Library has no written rule on executables inside a plugin, but it builds plugins from source with a C++ toolchain, and macOS would likely block an unsigned executable.

## The bridge module

Brumista is a TCP server inside Rack. Commands that touch the patch run on Rack's main thread; only plugin and model lists answer from the server thread. limen has the same shape: one file of about 1,165 lines (`src/limen.cpp` in forsitan modulare `master-v2`), with no DSP.

- **Server thread.** Listens on `127.0.0.1` only, one client at a time, with one JSON object per line. It polls its sockets every 100 ms so it can stop cleanly; on Linux, closing a socket from another thread doesn't reliably wake it. Sends never raise `SIGPIPE`, so a client that hangs up can't take Rack down. If the port is taken (a second Brumista), it retries every second.
- **Main-thread queue.** Anything that touches the engine or the scene, reads included, is queued as a function and run in the module widget's `step()`. Reading from the server thread races with the main thread removing a module. The server thread waits 2 s for the result.
- **Timeouts cancel.** A command still queued when the wait ends is cancelled and never runs, so a client told "timeout" can trust nothing happened. One the main thread has already started is waited for until it finishes. limen leaves timed-out work in the queue to run later.
- **Port.** 7200 by default, not limen's 7000, so both modules can sit in one patch while W72 is being built.
- **Cables as Rack makes them.** Every new `CableWidget` gets a colour: the requested one, or `RackWidget::getNextCableColor()`. limen does the same since forsitan 2.17.1; before that it set none, which left Rack's default of transparent black ([forsitan-modulare#24](https://github.com/gosub/forsitan-modulare/issues/24)).
- **Undo.** Each change pushes a Rack history action (cable add or remove, module add, parameter change), so Ctrl+Z works on Claude's edits.
- **Validate before acting.** Port indices are checked against what the panel actually exposes, and every command is wrapped so an exception becomes an error reply instead of a Rack crash. Through limen, a cable to a port with no panel jack (on NYSTHI Programmer) took Rack down.

## Features and milestones

The first milestone matches what limen does today, without its problems. Each later one adds a capability limen doesn't have. limen's surface (forsitan 2.17.2, protocol 2) is captured in `protocol/limen/commands.json`: each command's params, result shape and error strings, plus the problems Brumista must not repeat.

1. **Parity, done properly.** `hello`, list modules, params, ports and cables, add, move and remove modules, set params, add and remove cables, batch, save as. Plus cable colours, undo history and validation from the start.
    - Done when: a full patch section can be built end to end, with cables in the requested colours.
2. **Seeing state.** Read a module's JSON state live (`dataToJson`), which today needs a save and a parse of the `.vcv` file. Set a cable's colour after the fact.
    - Done when: a quantizer's scale mask or a mixer's gain trims can be read from a running patch.
3. **Building blocks.** Load a `.vcvm` preset into a module and add a `.vcvs` selection at a position. Stoermelder PackOne (8FACE, STRIP) is the prior art.
    - Done when: a saved selection can be dropped into a patch by name.
4. **Measuring.** Read port voltages and channel counts, with simple statistics over a short window (min, max, mean).
    - Done when: Claude can check that a quantizer output moves, or that a poly cable carries four channels, without asking you to listen.
5. **Seeing the rack.** Take a screenshot of the rack or a module, and open a patch.
    - Done when: Claude can look at a panel without you taking a screenshot.

Sound modules (Ghisa, Scighera, Gibigiana) come after milestone 1 at the earliest. Their specs come from technique cards.

## The MCP binary

The MCP server is a small Go program that speaks MCP over stdio to the client and the module's protocol over TCP to Rack. Users download one executable; nothing else to install.

- **Why Go.** One static executable, no runtime. It cross-compiles to Windows, Linux and macOS by setting `GOOS` and `GOARCH`. There's an official MCP SDK (`modelcontextprotocol/go-sdk`). Python was ruled out because users would have to install it.
- **Its job is thin.** Generate the tool list from `protocol/commands.json`, forward each call to the module, return the reply. No patch logic lives in the binary.
- **Its name.** The MCP server takes the bridge module's name, Brumista, which becomes the tools' prefix (`mcp__brumista__add_cable`).
- **Its tools.** One tool per command in `protocol/commands.json`, with the same name, description and parameters. `protocol/` is a small Go module that embeds the file, so the binary reads the one source of truth rather than a copy. A tool call returns the command's `result` as JSON text; an error reply or a failed connection comes back as a tool error the model can read.
- **One connection per call.** Each call connects, sends `hello`, sends the command and closes. Brumista serves one client at a time, so a held connection would lock out every other client. Calls from one binary run one at a time, since Brumista's listen backlog is 1 and a burst of parallel connections is refused.
- **The handshake.** The binary refuses a server whose `hello` doesn't name Brumista (limen answers `hello` too) and a module on another major version. Any other version mismatch works, with a warning on stderr.
- **Configuration.** `-host` and `-port`, defaulting to `127.0.0.1` and `7200`. Registered in Claude Code with `claude mcp add brumista -- <path>/brumista-mcp`.
- **Releases.** Pushing a tag `vX.Y.Z` that matches `plugin.json` runs `.github/workflows/release.yml`, which builds `brumista-mcp-X.Y.Z-<os>-<arch>` for Windows, Linux and macOS and publishes them, with `SHA256SUMS`, as a GitHub Release. `mcp/version.go` holds the version; a test checks it against `plugin.json`.
- **The one-artifact alternative.** The module could serve MCP over HTTP itself (localhost only, an `Origin` check and a token). That's kept in reserve, since two artifacts with a sync procedure are acceptable.

## Toolchain and build

The plugin builds natively on Windows with MSYS2 (MinGW64) and the Rack SDK, VCV's documented setup.

| Piece | What it is |
| --- | --- |
| MSYS2 (MinGW64) | Compiler, `make` and shell, installed with one `pacman` line |
| Rack SDK | Zip of headers and build files matching the Rack version (`Rack-SDK-2.x-win-x64`); `RACK_DIR` points at it |
| Go | Builds the MCP binary for all platforms |

The build loop:

1. Close Rack, since Windows locks a loaded plugin `.dll`.
2. `make install` at the repo root, which builds and copies the plugin into Rack's user `plugins-win-x64/` folder.
3. Start Rack and test.

Cross-compiling from Linux (VCV's `rack-plugin-toolchain`, or mingw-w64 with the `-posix` g++) is the alternative. It's worth it only to check Linux and macOS builds before a Library submission, and the Library builds those itself.

Changes land through pull requests with Conventional Commits messages. A lightweight GitHub Actions CI is planned once there is code: build and test the Go server, check `commands.json` against both sides, and compile the plugin on Linux against the Rack SDK. Release builds of the binary come from the same Actions setup.

## Where work runs

Development runs as a Claude Code Project: a coordinator plus threads. Work that needs Rack or the Windows build runs in local threads on the development machine, and everything else can run in the cloud.

| Work | Thread | Why |
| --- | --- | --- |
| Design, specs, docs, reviews | Cloud | Needs only the repo |
| Go MCP binary: code, tests, cross-builds | Cloud | Builds anywhere |
| C++ code that a compile check can verify | Cloud | A Linux compile catches most errors |
| Windows build and `make install` | Local (Remote Control) | Needs MSYS2 and the Rack folder |
| Live tests against a running Rack | Local (Remote Control) | Needs Rack, limen and rackmaster |

## How the project uses rackmaster

rackmaster, a Claude Code plugin for VCV Rack, is the project's main source of context: a local index of installed modules with manuals, technique cards and errata. Every thread checks it before writing code that something else may already do.

| Part | Used for |
| --- | --- |
| Module index | Name and slug collisions; gap checks, so sound modules fill gaps instead of adding a twenty-first shimmer |
| Manuals and source | Prior art: Stoermelder PackOne for presets and module state; Plateau for reverb structure; existing shift registers for Ghisa |
| Technique cards | Specs for sound modules; the patches they were found in become test patches |
| Errata and conventions | A checklist of mistakes not to repeat |

Errata that become rules for W72:

- **Refuse cables to hidden ports**, from the NYSTHI Programmer crash.
- **Declare ports in enum order**, from Cella Resonators, whose port ids disagree with its declaration order.
- **Show momentary state**, from Dopamine's Record param, which reads 0 at rest.
- **Expose real state to the patch**, from quantizers whose scale mask is invisible to parameters.

W72 modules also get rackmaster manuals from the start, written next to the code.

## Licence and naming

The plugin and the MCP binary are GPL-3.0-or-later. That allows reusing limen, limen-tools and Stoermelder code, which are all GPL-3, and it's the licence VCV recommends.

- **Slugs:** plugin `W72`. Module slugs in plain ASCII, permanent once a patch uses them.
- **No "VCV"** in any name, slug or binary name.
- **The binary's name** follows the bridge module, for example `brumista-mcp`.
- **Naming grammar and glossary:** in [concept.md](concept.md).

## Decisions

| Decision | Date |
| --- | --- |
| Repo `w72-rack-plugin`; plugin slug and name `W72` | 2026-11-01 |
| Port 7200; the MCP server takes the bridge module's name | 2026-11-01 |
| `commands.json` is a test oracle; generate only if drift becomes a problem | 2026-11-01 |
| No executable in `res/`; the binary ships on GitHub Releases | 2026-11-01 |
| Plugin at the repo root, so the Library can build it | 2026-11-01 |
| Milestones: parity, seeing state, building blocks, measuring, seeing the rack | 2026-11-01 |
| GPL-3.0-or-later for plugin and binary | 2026-11-01 |
| Pull requests for every change; Conventional Commits | 2026-11-01 |
| Semantic Versioning, one version for plugin, binary and protocol (`2.MINOR.PATCH`) | 2026-11-01 |
| Lightweight GitHub Actions CI once there is code | 2026-11-01 |
| The bridge module is Brumista (slug `Brumista`); the MCP server takes the same name | 2026-11-01 |
| Panels are dark only; the W72 mark is the only element taken from the W72 web design system | 2026-11-01 |
| Panel style Metropolitana: a family-coloured band, Inter Tight outlines, a light output plate ([design-system.md](design-system.md)) | 2026-11-01 |
| Panel design is decided on a Claude Design canvas; panel SVGs are built by a generator in the repo | 2026-11-01 |
| The panel generator is written in Go | 2026-11-01 |
| Panel descriptions place controls by row (counted from the bottom) and order, never by coordinate | 2026-11-01 |
| Control positions reach the C++ through a generated `src/<Slug>Panel.hpp`, not a hidden SVG layer | 2026-11-01 |
| Static Inter Tight files (OFL) live in `tools/panelgen/fonts/`, outside the plugin package | 2026-11-01 |
| limen's command surface is captured in `protocol/limen/commands.json` as the parity oracle, apart from W72's own `protocol/commands.json` | 2026-11-01 |
| Every command that touches the engine or the scene runs on the main thread, reads included | 2026-11-01 |
| A command that times out before the main thread starts it is cancelled and never runs | 2026-11-01 |
| `hello` reports `server` and the semver `version`, not an integer protocol number | 2026-11-01 |
| The MCP binary uses the official Go SDK (`modelcontextprotocol/go-sdk`) and embeds `commands.json` through a `protocol` Go module | 2026-11-01 |
| The MCP binary opens one connection per call and runs its calls one at a time | 2026-11-01 |
| Release binaries are plain executables per platform, published by a tag-triggered workflow | 2026-11-01 |
| The first release is v2.1.0, tagged when Brumista reaches parity with limen (milestone 1) | 2026-11-01 |

## Open questions

None for now.
