# Connecting to other software

[← MIDI output and instruments](08-midi-output.md) · [Contents](README.md) · [Running as an LV2 plugin →](10-lv2-plugin.md)

Luvie produces MIDI; something else has to turn it into sound. These are recipes
for the combinations that come up most often.

## Bridging ALSA and JACK MIDI

```
a2jmidid -e
```

## Carla, over ALSA

Use Carla's patchbay. Press <kbd>Ctrl</kbd>+<kbd>R</kbd> to refresh the patchbay
after starting Luvie, or the new ports will not appear.

## Carla, over JACK with PipeWire

Carla cannot start JACK itself, which is a nuisance when you would like all of
your project settings stored in one place. Modern PipeWire removes the need:

```
pw-jack carla myProject.carxp
```

Then, in Carla:

1. Set the engine to JACK.
2. Set JACK to auto-connect.
3. Restart the server.
4. Check **Use JACK Transport** — this is what enables Luvie's transport
   controls.

Everything is then stored under one roof.

TODO: verify this recipe on a current PipeWire; it was written from a setup that
did not yet have one.

## Seeing what is connected

Either of these shows the graph:

```
qpwgraph
helvum
```

## Other hosts

TODO: Ardour, Reaper, and whatever else gets tested. Carla classifies Luvie as
"Other" rather than as a MIDI plugin — worth a note here so people can find it
in the plugin list.


# Running as an LV2 plugin

[← Connecting to other software](09-connecting.md) · [Contents](README.md) · [Sessions and saving →](11-sessions.md)

The same sequencer runs either as a standalone application or as an LV2 plugin
inside a host. The plugin exists mainly to simplify session management: the host
saves Luvie's state along with everything else in the project.

TODO: where to put the bundle — cross-reference
[Installation](01-installation.md).

## Finding it in your host

Luvie has a MIDI output and no MIDI input, and some hosts classify plugins by
their ports rather than by what the plugin says it is. Carla, for one, files
Luvie under "Other" instead of under MIDI plugins, and hides it when that filter
is off.

TODO: the equivalent for Ardour and other hosts as they are tested.

## What differs from the standalone application

TODO: transport follows the host; MIDI leaves through the plugin's output port
rather than through Luvie's own ports; state is saved by the host.

TODO: whether the output port list is available at all in plugin mode.
