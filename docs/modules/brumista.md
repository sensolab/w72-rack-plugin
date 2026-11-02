# Brumista

*The old Milanese cab driver: it carries commands into a running Rack, where it's told.*

Brumista is a bridge. It runs a small server inside Rack that lets an MCP client such as Claude Code read the patch and edit it: list modules, ports, parameters and cables, add and move modules, set parameters, connect cables and save. It makes no sound and has no inputs or outputs.

- **Family:** trades · **Width:** 3 HP · **Slug:** `Brumista` · **Since:** 2.1.0

## Getting started

1. Add Brumista to the patch. It starts listening on `127.0.0.1:7200` at once.
2. Get `brumista-mcp` for your platform from [GitHub Releases](https://github.com/sensolab/w72-rack-plugin/releases), or build it from `mcp/` with Go (`go build -o bin/brumista-mcp .`).
3. Register it with Claude Code:

   ```
   claude mcp add brumista -- /path/to/brumista-mcp
   ```

The tools appear as `mcp__brumista__<command>`, one per command below. `-host` and `-port` point the binary at another address; `-version` prints its version.

One Brumista per patch is enough. A second one finds the port taken, stays dark and retries every second, so it takes over if the first is removed.

## Panel

![Brumista panel](../../res/Brumista.svg)

| Control | What it shows |
| --- | --- |
| LINK light | Off: the port is taken (by another Brumista or another program) and Brumista retries every second. Dim: listening, no client. Full: a client is connected. |

The MCP binary opens one connection per tool call, so the light flashes to full during a call and drops back to dim.

## Commands

Every command is a JSON object on one line, `{"cmd": "<name>", ...params}`, and every reply is `{"ok": true, "result": ...}` or `{"ok": false, "error": "..."}`. The full list, with parameters, result shapes and error strings, is in [protocol/commands.json](../../protocol/commands.json), which the MCP binary builds its tools from.

| Group | Commands |
| --- | --- |
| Handshake | `hello` |
| Reading | `list_plugins`, `list_models`, `list_modules`, `get_module`, `get_module_info`, `list_ports`, `list_params`, `get_param`, `get_module_data`, `list_cables` |
| Editing | `set_param`, `add_module`, `remove_module`, `move_module`, `add_cable`, `remove_cable` |
| Several at once | `batch` |
| Saving | `save_patch`, `save_patch_as` |
| View | `set_fullscreen`, `zoom_to_modules` |

How it behaves:

- **Every edit can be undone.** Each write records one step in Rack's Edit > Undo, so Ctrl+Z takes it back like a hand edit. A `batch` is a single step, whatever it holds. Saving and the view commands aren't edits and record nothing.
- **Cables always have a colour.** `add_cable` uses the colour it's given (`#rrggbb` or `#rrggbbaa`), or Rack's next cable colour, as a dragged cable would. `list_cables` reports each cable's colour.
- **Reads show what the panel shows.** `list_params` gives each parameter's value as the module displays it (`"440.0 Hz"`, or a switch's label), its default and a switch's labels. `list_ports` gives each port's channel count and voltages. `get_module_data` reads the state a module saves beyond its parameters, such as a quantizer's scale mask.
- **Voltages are one instant.** A voltage is a single sample taken when the call runs: right for a pitch CV, a held gate or an offset, meaningless for an audio-rate signal, and a short trigger is easily missed.
- **Parameters behave like knobs.** `set_param` clamps to the parameter's range, rounds stepped parameters, and replies with the value that resulted.
- **Ports are checked against the panel.** A cable to a port the module declares but has no jack for is refused instead of crashing Rack.
- **Saves don't overwrite by accident.** Paths must be absolute and get `.vcv` when missing. An existing file is replaced only when the call sets `overwrite`. `save_patch` with no path is Rack's Save; `save_patch_as` is Save as.
- **A timeout means nothing happened.** Commands that touch the patch run on Rack's main thread, which has 2 s to start them. A command still waiting when that runs out is cancelled, never run later.
- **Brumista won't remove itself.** `remove_module` refuses a Brumista module, since the server runs inside it.

There is no `quit`: closing Rack from outside skips its save prompt.

## Security

Brumista listens on the loopback address only (`127.0.0.1`), so other machines cannot reach it. It has no authentication: any program on the same machine can connect, then edit the patch, change parameters and save files. Add it to a patch only when that is acceptable, and remove it when it isn't needed.

## Relation to limen

Brumista speaks the command protocol of limen, the bridge module in gosub's [forsitan modulare](https://github.com/gosub/forsitan-modulare), so the commands, parameters, reply shapes and error strings match, and a client written for limen works against Brumista on port 7200. Its behaviour was checked against a running limen (forsitan 2.17.2), and limen's surface is recorded in [protocol/limen/commands.json](../../protocol/limen/commands.json) as the reference. The code is W72's own; the server's socket loop follows the shape of limen's, as the source files' headers note. Both modules are GPL-3.0-or-later.

Where Brumista departs from limen:

| | limen | Brumista |
| --- | --- | --- |
| Port | 7000 | 7200, so both can run in one patch |
| `hello` | `{protocol: 2, commands}` | `{server: "Brumista", version, commands}`, with the plugin's semver |
| Port with no panel jack | Accepted; can crash Rack | Refused with an error |
| `set_param` | Raw value from the server thread; returns null | Clamped and snapped on the main thread; returns the parameter |
| Reads | Several from the server thread | Everything that touches the patch on the main thread |
| Undo | None | One step per write, one per `batch` |
| `batch` | Each write sent to the main thread on its own | One main-thread task, no frame in between |
| Saving | Overwrites silently; leaves the patch marked unsaved | Refuses to overwrite unless `overwrite`; marks the patch saved |
| `list_cables` | No colour | Includes each cable's colour |
| `list_params`, `list_ports` | Value, name, range, unit; port names | Adds display strings, defaults, switch labels, channel counts and voltages |
| Module state | No way to read it | `get_module_data` reads `dataToJson` live |
| Timeouts | Timed-out work still runs later | Cancelled before it starts |
| `remove_module` | Leaves a gap | Rack's own Delete, which closes the gap when squeeze is on |
| `quit` | Closes Rack without a save prompt | Not implemented |

Each difference is listed command by command under `limenDiff` in [protocol/commands.json](../../protocol/commands.json).
