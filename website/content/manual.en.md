---
title: Manual
description: How to use DJ-Workstation – players, crossfader, library, playlists, controllers and settings.
layout: manual
---

DJ-Workstation is made for touchscreens first: every control works with a finger, and a mouse works the same way. Rest
the mouse on a control and a short help appears. The player's controls keep the DJ's English words (Load, CUE, KEY,
Pitch) in every language.

## Contents

- [First start](#first-start)
- [The player](#the-player)
- [Beat waveforms and crossfader](#beat-waveforms-and-crossfader)
- [The library](#the-library)
- [Playlists](#playlists)
- [Windows and layouts](#windows-and-layouts)
- [Keyboard shortcuts](#keyboard-shortcuts)
- [MIDI controllers](#midi-controllers)
- [Settings](#settings)

## First start

While the library has no music folder yet, DJ-Workstation asks for one at the start. It then reads all tracks in that
folder and its subfolders (MP3, Ogg Vorbis, FLAC and WAV) together with their tags. You can add more folders later with
**File > Add music folder…**.

Without a saved layout the program opens two players with the beat waveforms between them and the crossfader below.

## The player

{{< callouts name="player" alt="A player with numbered marks on its controls" >}}

1. **Load** puts the track selected in the library into this player. You can also double tap a track in the library
   (it goes into a free player) or drag a file from your file manager onto the player. While the player plays, Load is
   dimmed: a playing track is never replaced.
2. **− / +** slow the track down or speed it up for as long as you hold them, to nudge two tracks back into step.
3. **KEY** switches keylock on: the tempo changes with the pitch fader, the key stays.
4. **Time** switches the display between elapsed and remaining time.
5. **BPM** is the track's tempo, with the pitch taken into account.
6. **The waveform** shows the whole track; the part already played turns yellow. Tap it to jump there. On a stopped
   player you hear a short loop at the place you touch, so you can find a spot by ear; CUE then sets the cue point
   there.
7. **The state** of the player: loaded, cued, paused or playing.
8. **CUE** works as on a CD player: while the player stands it sets the cue point at the current position; while it
   plays it jumps back to the cue point and stops; held down, it plays from the cue point until you let go.
9. **Play** starts and pauses the track.
10. **The pitch fader** changes the tempo by up to ±10 % in steps of 0.02 %. As on a turntable, down is faster. A
    double tap sets it back to 0.
11. **Vol** is the channel volume; the meter in its groove shows the level.
12. **Headphone cue** sends the player to the headphones; its light is on while it does.
13. **Head** is the headphone volume.
14. **Artist and title** of the loaded track.
15. **Cue-Load**: when on, a loaded track starts at the cue point you last set for it. A track without one starts at
    its first sound instead of at the silence before it.

## Beat waveforms and crossfader

{{< callouts name="mixer" alt="Beat waveforms and crossfader with numbered marks" >}}

1. **Beat waveforms** show six seconds of the players on sides A and B: bass in red, mids in green, highs in blue.
2. **The fixed line** is where the music plays now. When the beats of both lanes meet it together, the tracks run in
   step.
3. **This button** turns the lanes from vertical to horizontal and back.
4. **The two drop-downs** choose which player sits on side A and which on side B.
5. **Zero** puts the crossfader in the middle.
6. **The meter** shows the master level.
7. **A** and **B** jump to that side while you hold them and go back when you let go.
8. **The crossfader** blends from side A to side B.
9. **Fade now!** fades over to the other side on its own.
10. **Fade time (s)** sets how long that fade takes.

## The library

{{< callouts name="library" alt="The library with numbered marks on menu bar, search, filters and track list" >}}

1. **The menu bar**: File, Playlist, Settings, View and Layout.
2. **Search** finds tracks as you type, in all fields at once. Typing while no text field is selected also goes in
   here, so you can just start typing.
3. **The column filters** search one column each: artist, title, genre and year. `*` stands for any text, `?` for one
   character.
4. **A column header** sorts the list by that column.
5. **The track list**: tap a track to select it, double tap it to load it into a free player.
6. **The number of tracks** found, and what the library is doing (scanning, analyzing).

Right-click a track for more:

- **Edit tags** changes artist, title, genre and year in the file and in the library. The tags of a track that is
  playing cannot be changed.
- **Scan again** reads the file's tags once more and analyzes it anew.

The tempo (BPM) and the waveform of a track are found when you load it for the first time. **File > Analyze all
tracks** does it for the whole library in the background, while you go on working.

**File > Rescan library** looks for new, changed and deleted files. Files that are gone are counted; **Remove missing**
takes them out of the library.

## Playlists

Open the playlist with **View > Playlist window**. **New** creates a playlist, **Add to playlist** in the library adds
the selected track, **Up** and **Down** change the order and **Remove** takes a track out. Playlists are kept in the
library and can be imported and exported as M3U files (**Import**, **Export**).

## Windows and layouts

Each player, the crossfader, the beat waveforms and the playlist can be a window of its own, moved and resized freely,
also onto another screen. **View > One-window mode** puts them all into one window instead (from the next start).
**View > New player** opens another player; there is no fixed limit. **View > Fullscreen** (Ctrl+F) fills the screen,
for touch terminals.

In the one window, **Layout > Edit layout** lets you drag a part to move it or drag its lower right corner to resize it;
edges snap to their neighbours. **Layout > Save layout** keeps the arrangement for the next start, **Layout > Reset to
default** goes back to two players with the crossfader.

**Layout > Save layout as…** writes the arrangement to a file of its own, for example one per venue or screen;
**Layout > Load layout…** brings it back at once and keeps it for the next start. A layout made in the other window
mode shows from the next start.

**View > Daylight colors** switches to light colors for bright rooms or sunlight.

While a track plays, **File > Quit** asks first, so a set does not end by accident.

## Keyboard shortcuts

They act on the player that has the keyboard focus and always use the left Alt key (Option on the Mac), so typing a
search never reaches a player.

| Keys | Action |
|---|---|
| Alt+P | Play / pause |
| Alt+C | Cue (acts while held) |
| Alt+L | Load the selected track |
| Alt+← / Alt+→ | Step back / ahead (with Shift: 10 seconds) |
| Alt+↑ / Alt+↓ | Pitch ±0.02 % (with Shift: ±0.1 %) |
| Ctrl+F | Fullscreen |

## MIDI controllers

Controllers can be plugged in and out while the program runs. **Settings > Devices** assigns a mapping to each
controller. A mapping is a small TOML file that ties the controller's buttons, faders and jog wheels to play, cue,
load, pitch, volume, crossfader and more, and lights its LEDs. The format is described in
[mappings/README.md](https://github.com/sagiadinos/dj-workstation/blob/main/mappings/README.md).

**Import Mixxx mapping…** turns a Mixxx mapping (.midi.xml) into one of ours. Its direct assignments are taken over;
what Mixxx does with scripts is left out and counted.

The Hercules DJControl Starlight is recognized by name, with its sound card: master on outputs 1/2, headphones on 3/4.
Its jog wheels bend the pitch when turned at the edge and scratch when touched on top.

## Settings

**Settings > Devices**: the output device, stereo or four channels (master on 1/2, headphones on 3/4), the latency
(shorter reacts faster, longer guards against dropouts) and the controller mappings. Audio settings take effect at the
next start.

**Settings > Sound**: the keylock quality, High (the default) or Standard for slow computers.

**Settings > Language**: English, German, French, Spanish, Greek or Russian; the change shows at once.
