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
