# The pattern editors

[← Harmony patterns](09-harmony-editor.md) · [Contents](README.md) · [The drumkit editor →](11-drumkit-editor.md)

## The pianoroll editor

The pianoroll editor is a fairly conventional editor, similar those used in many
other MIDI sequencers. Being the simplest of the pattern editors we'll also use it 
to demonstrate the features common to all of the pattern editors.

Each row represents one semitone, labelled with its note name. A parameter lane (Modulation, above) can be shown
beneath the grid.

<img src="images/pianorollEditor.png" alt="The Luvie pianoroll editor" width="800">

## Adding and removing notes

To add a note simply click somewhere on the grid. To remove it you can hover over the
note and press delete or right click on it and choose delete from the menu.

You can change the length of a note by clicking the left or right hand side of it and
dragging.

Let's say you want your notes to be longer or shorter... change the 'Div' setting in the
timing panel, or click on 'Snap' to disable it.

## Velocity

The 'velocity' of a note is how loud the note is in MIDI-speak. 
It's obscure terminology but don't blame me, I just work here.

To change it right click on a note and adjust using the slider.

TODO screenshot



NOTE: the other pattern editors behaviour similarly

## Automation

MIDI parameters such as pitch bend, modulation, etc can be automated using rubberbands in dedicated automation lanes. 
See the screenshot above for an example. Note that this automation applies at a pattern level, but the song editor has its own automation lanes

To create a lane right-click on the track in the left hand column and choose 'Add automation'.

TODO screenshot

You can add dots on the rubberbands by clicking on it. You can click and drag them around, and you can delete them
either by hovering over them and pressing delete or by right-clicking on them and using the menu option.

## Main settings panel/row

TODO instrument name etc  NB changing name

## Timing panel 
- Why the odd divisions? NB snap


