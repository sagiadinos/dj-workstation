# Controller mappings

A mapping tells djw what the buttons, faders and knobs of a MIDI controller do and which of its lights to switch. It is a TOML file, one per controller. [hercules_djcontrol_starlight.toml](hercules_djcontrol_starlight.toml) is a complete example.

## Where mappings are

- `mappings/` next to the program: the mappings that come with djw.
- `mappings/` in the user's data folder (e.g. `~/.local/share/djw/mappings` on Linux): your own, and those made by the Mixxx import.

Every `*.toml` file in both folders is read at the start; a file with a mistake is skipped and the error reported. A controller's MIDI port gets the first mapping whose `match` fits its name, or the one chosen for it in Settings > Devices.

## Structure

```toml
[device]
name  = "Hercules DJControl Starlight"   # shown in Settings > Devices
match = ["DJControl Starlight"]          # parts of the MIDI port name, ignoring case
audio = "DJControl Starlight"            # optional: part of the name of its sound card

[[control]]
midi   = { type = "note", channel = 2, number = 0x07 }
action = "deck.play"
deck   = 1

[[light]]
what = "deck.playing"
deck = 1
midi = { type = "note", channel = 2, number = 0x07 }
```

### `[device]`

| Key | | |
|---|---|---|
| `name` | required | The name of the mapping. |
| `match` | required | A list of texts; the mapping is for every MIDI port whose name contains one of them. |
| `audio` | optional | Part of the name of the controller's own sound card. With the audio output on Automatic, djw then uses that card with master on 1/2 and headphones on 3/4. |

### `[[control]]`: a button, fader or knob

| Key | | |
|---|---|---|
| `midi` | required | `{ type = "note" or "cc", channel = 1 to 16, number = 0 to 127 }`. Numbers may be written in hex (`0x07`). |
| `action` | required | What it does, see below. |
| `deck` | for `deck.*` | The player, counted from 1. A player that does not exist ignores it. |
| `mode` | optional | How the value is read, see below. `button` if left out. |
| `shift` | optional | `true`: applies only while a `shift` control is held, instead of without it. |
| `invert` | optional | `true`: absolute modes run the other way, for a fader built upside down. |

Modes:

| Mode | For | Value |
|---|---|---|
| `button` | buttons | Pressed (note on, or cc above 0) and released (note off, or cc 0). |
| `absolute` | faders, knobs | 0 to 127. |
| `absolute14` | faders with 14 bits | A cc below 32 is the high part, the same cc + 32 the low part. |
| `relative` | encoders, jog wheels | Steps: 1 to 63 forward, 127 down to 65 back. |

Actions:

| Action | Mode | What it does |
|---|---|---|
| `shift` | button | While held, the controls with `shift = true` apply instead of the others. Not for a controller that handles Shift itself and sends other messages then (the Starlight does). |
| `deck.play` | button | Play/pause. |
| `deck.cue` | button | CUE as on a CDJ, acts while held. |
| `deck.load` | button | Loads the track selected in the library (not while the player plays). |
| `deck.keylock` | button | Keylock on/off. |
| `deck.headphones` | button | Headphone cue on/off. |
| `deck.bend_down`, `deck.bend_up` | button | Pitch bend while held. |
| `deck.pitch` | absolute | Pitch fader, ±12 %: the highest value is the fastest. As on a turntable, pulled down should be faster; a fader that sends its highest value at the top needs `invert = true` (the Starlight's has it). |
| `deck.volume` | absolute | Channel fader. |
| `deck.headphone_level` | absolute | How loud the player is on the headphones. |
| `deck.seek` | relative | Steps back or ahead, like the cursor keys of a player (one scrub loop of 60 ms per step). |
| `deck.jog` | relative | Jog wheel: while the player plays it bends the pitch as long as it turns; while it stands it steps like `deck.seek`. |
| `mixer.crossfader` | absolute | Crossfader, 0 side A to the end side B. |
| `library.move` | relative | Moves the selection in the library. |

A button is released as what it was pressed as, even if Shift was let go in between.

### `[[light]]`: an LED

| Key | | |
|---|---|---|
| `what` | required | `deck.playing` (the player plays), `deck.cue` (as on a CDJ: lit at the cue point and while held, blinking when paused elsewhere) or `deck.headphones` (headphone cue on). |
| `deck` | required | The player, counted from 1. |
| `midi` | required | The message that switches it, as for a control. Sent to the MIDI output with the same name as the input. |
| `on`, `off` | optional | The values sent, `0x7F` and `0x00` if left out. |

A light is only sent when it changes, and switched off when djw closes.

## Finding the numbers

On Linux `aseqdump -l` lists the MIDI ports and `aseqdump -p <port>` shows what every button and fader sends. A Mixxx mapping (`.midi.xml`) of the controller is also a good source: Settings > Devices > Import Mixxx mapping… turns its direct bindings into a djw mapping in the user's folder; what its script does cannot be taken over.

## Not yet

Scripts (Lua), MIDI learn and hotplug are planned, see [FEATURES.md](../FEATURES.md).
