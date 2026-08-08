# MIDI output and instruments

[← Loops](07-loops.md) · [Contents](README.md) · [Connecting to other software →](09-connecting.md)

TODO: screenshot of the outputs panel.

## Output ports

Each port sends to one destination. Luvie can drive JACK MIDI ports, the
platform's native MIDI interface, or a debug output that prints what would have
been sent.

TODO: adding a port, choosing its backend, and what appears at the other end for
each.

TODO: which backends exist on which platform.

## Selecting an instrument

TODO: write the instructions about bank select MSB/LSB and program selection.

The program number does **not** have to be set. Left unset it uses whatever
instrument the device is currently on, and "none" is not the same as 0. Plenty
of devices have neither banks nor programs at all — a Yamaha Reface CS, for
instance — and for those, none of this needs touching.

## Drum kits

TODO: naming drum instruments and reusing a kit across patterns.
Cross-reference [The pattern editors](05-pattern-editors.md).
