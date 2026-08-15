# MIDI output and instruments

[← Getting started](02-basics.md) · [Contents](README.md) · [A simple walk through →](04-walkthrough.md)

The gear icon at the top right of the window opens the **Instruments & Outputs**
dialog, where everything in this chapter is set up:

<img src="images/settings.png" alt="The Instruments and Outputs dialog" width="800">

## MIDI Output ports and channels

Instrument settings ... Complicated? You better believe it...

OK, so the world of MIDI outputs is complicated, but Luvie is set up in such a way
as the isolate the complexity in this one GUI and then the rest of the app can 
continue on in blissful ignorance.

Note too that most of the settings are not required most of the time.

## Instruments

Instruments are Luvie's way of bringing some sanity to the work of MIDI outputs.
You take a MIDI Output Port and specify a MIDI channel and give it an instrument name.
Then throughout the rest of the application you can refer to instruments by name and
forget about MIDI ports and channels.

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
Cross-reference [The drumkit editor](11-drumkit-editor.md).
