# MIDI input, output and instruments

[← Getting started](02-basics.md) · [Contents](README.md) · [The song editor →](04-song-editor.md)

The gear icon at the top right of the window opens the **Instruments and I/O**
dialog, where everything in this chapter is set up:

<img src="images/settings.png" alt="The Instruments and I/O dialog" width="800">

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
platform's native MIDI interface, a debug output that prints what would have
been sent, or — when Luvie is running as an LV2 plugin — one of the plugin's own
MIDI outputs.

Which of those a port can use depends on how Luvie is running, so the dropdown
only offers the ones that work here and greys out the rest:

- **Standalone**: Jack, Native and Debug. Plugin is greyed out.
- **As an LV2 plugin**: Plugin only. Jack, Native and Debug are greyed out —
  the host owns the audio thread, and the plugin has no ports of its own outside it.

A port keeps whatever backend it was saved with even when this mode cannot drive
it, so moving a project between the standalone app and a host does not lose the
setting.

### Plugin outputs

The plugin has eight MIDI outputs, and the ports set to **Plugin** are handed to
them in list order: the first such port drives *MIDI Out 1* in the host, the
second drives *MIDI Out 2*, and so on. Ports on any other backend, and anything
past the eighth, fall back to *MIDI Out 1*.

Because the mapping is positional there is nothing to name, so a Plugin port is
listed under the output it drives — *MIDI Out 1*, *MIDI Out 2* — and its name box
is greyed out. Instruments show the same name, so both halves of the panel agree
on where a note ends up.

That name is only what you see here. The port keeps whatever name it was given,
so a project taken back to the standalone app still has its own port names and
switching a port to Jack, Native or Debug shows them again.

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
Cross-reference [The drum pattern editor](08-drum-pattern-editor.md).

## MIDI input

At the bottom of the window is the MIDI input. There is exactly one, and it has
two settings:

**Type** — where the input comes from, on the same terms as the output ports, so
only the ones this mode can actually use are offered:

- **Standalone**: Jack and Native. Plugin is greyed out.
- **As an LV2 plugin**: Plugin only. Jack and Native are greyed out — the host
  owns the connection. The plugin has one MIDI input, shown by the host as
  *Control In* (or *events-in* in Carla); connect your keyboard to that and the
  notes arrive.

There is no Debug type here: Debug is a place to send MIDI to, not somewhere it
can come from. As with the output ports the setting is kept even when this mode
cannot use it, so moving a project between the standalone app and a host does
not lose it.

**MIDI channel** — which channel to listen on. The default, **Any**, accepts
everything, which is what you want for a single keyboard. Set it to 1-16 to
ignore everything else, which is useful when a controller sends on several
channels at once, or when something else is sharing the port.

The Jack and Native inputs both appear to the outside world as a port named
`midi_in`, which is what you connect your keyboard to — `midi_in` in a JACK
patchbay, or `luvie:midi_in` in an ALSA one.

Incoming notes go to whichever pattern editor is open, and are played on that
pattern's instrument so you hear what you are playing. Recording them is
described in [The pattern editors](06-pattern-editors.md).
