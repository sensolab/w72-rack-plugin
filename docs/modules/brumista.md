# Brumista

*Lets an AI agent read and patch your running Rack.*

## What it does

Brumista connects Rack to an AI agent, such as Claude Code, through the Model Context Protocol (MCP). With Brumista in the patch, you can ask the agent what's in your rack. It can add modules and cables, turn knobs, save the patch, and look at screenshots.

Everything the agent does lands in Rack's Edit > Undo, one step per change, so Ctrl+Z takes it back. Cables it adds always get a colour, and it can't patch a jack the panel doesn't show.

Brumista makes no sound and has no jacks. It needs a small companion program, `brumista-mcp`, which your agent runs. That program isn't in the VCV Library, so you download it once from GitHub. Brumista is 3 HP wide.

Brumista only listens on your own machine, and it has no password. Any program on the same computer can use it to change and save your patches. Remove it from the patch when you don't need it.

## The panel

![Brumista panel](brumista.png)

| Control | What it does |
| --- | --- |
| LINK light | Dim: waiting for the agent. Full: the agent is connected. Slow blink: the port is taken, and Brumista keeps retrying. |
| CMD light | Flashes for each command. Dim when the agent reads, full when it changes the patch. |

LINK flashing to full and back to dim is normal. The agent connects once per request.

## Right-click options

The default port suits almost everyone.

| Option | What it does | Default |
| --- | --- | --- |
| Port | The port Brumista listens on. Change it only if 7200 is taken, then give the agent the same one. | 7200 |

## Setting it up

### Start here: connect Claude Code

1. Add Brumista to the patch. Its LINK light turns dim.
2. Download `brumista-mcp` for your system from [GitHub Releases](https://github.com/sensolab/w72-rack-plugin/releases). Use the same version as the plugin.
3. Register it with your agent. For Claude Code:

   ```
   claude mcp add brumista -- /path/to/brumista-mcp
   ```

4. Ask the agent what's in your patch.

You now have an agent that can see and edit the rack you're playing.

### More ideas

**Read a shared patch.** Open a patch someone sent you, and ask the agent to walk you through it. It can say what each module does and where the cables go.

**Build a starting point.** Ask for a voice, a sequencer and a mixer, patched and ready, then take it from there by hand.

**Check what you're hearing.** Ask for a screenshot of a module, or the value of each knob on it.

**When the light blinks slowly.** Another program, or a second Brumista, has the port. One Brumista per patch is enough. To use another port, set it in the right-click menu and start `brumista-mcp` with `-port` and the same number.

**When nothing moves.** If Rack's audio module has no device, Rack's engine stops, and knobs and voltages stand still. The agent is told when that happens.

## Where it comes from

![Public horse-drawn cabs in Piazza del Duomo, Milan](brumista-photo.jpg)

*Public horse-drawn cabs in Piazza del Duomo, Milan, c. 1928–1935.
Photo: Giulio Galimberti. Civico Archivio Fotografico, Comune di Milano (FM C 225/2).
Reproduced for non-commercial purposes; not covered by the plugin's license.*

*Brumista: the old Milanese cab driver. It carries commands into a running Rack, where it's told.*

I started W72 with this module. I was using limen, the bridge in gosub's forsitan modulare, to let an agent patch my rack, and I kept hitting the same problems. Cables came out invisible, a cable to a hidden jack could crash Rack, and nothing could be undone.

Brumista speaks limen's protocol, so a client written for limen works with it, on its own port. It adds undo, cable colours, safe saves, module state and screenshots. Both can run in the same patch.

The code is W72's own. The server's socket loop follows the shape of limen's (GPL-3.0-or-later), as the source files note.

The full command list, how each behaves, and every difference from limen are in the [reference](brumista-reference.md).
