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
