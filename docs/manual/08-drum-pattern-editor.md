# The drum pattern editor

[← Harmony patterns](07-harmony-editor.md) · [Contents](README.md) · [BPM and time signatures →](09-beats-and-times.md)

<img src="images/drumsEditor.png" alt="The Luvie drum pattern editor" width="800">

The drum pattern editor gives each drum instrument a row, with S and M buttons for
soloing and muting them. 

Notes are single hits lacking a sustain. They can be added and removed by single clicking, 
and the velocity changed via a right-click.

The names of individual drums can be modified by right clicking the labels in the left column.
These names will of course be saved in the project, but it is also possible to export a drum map for use in
other projects via the "Instruments and I/O" window.

The "Instruments and I/O" window contains a few features to help populate the drum names.
It is possible to load drum maps, to use the standard GM or GS maps, as well as to list rows
by number of note name.

## Recording

The drum editor takes a MIDI keyboard or pad the same way the pianoroll does —
see [Recording from a MIDI keyboard](06-pattern-editors.md#recording-from-a-midi-keyboard).
Drum maps are keyed by MIDI note already, so a pad lands on the row its note
names. The one difference is that how long you hold a pad makes no difference: a
drum note has no length, so it is written the moment the pad goes down.
