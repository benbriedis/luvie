# The pattern editors

[← Harmony patterns](09-harmony-editor.md) · [Contents](README.md) · [The drumkit editor →](11-drumkit-editor.md)

## The pianoroll editor

The pianoroll editor is a fairly conventional editor, similar those used in many
other MIDI sequencers. Being the simplest of the pattern editors we'll also use it 
to demonstrate the features common to all of the pattern editors.

Each row represents one semitone, labelled with its note name. A parameter lane (Modulation, above) can be shown
beneath the grid.

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

## Control row

The main controls of the pattern editors are found at the bottom of the UI.

<img src="images/patternEditorControls.png" alt="The shared pattern editor controls" width="800">

The first control scrolls the grid to show the notes, and the second zooms the grid out horizontally.

Next comes the pattern name. Double click to edit.

The next control is a dropdown containing the instrument name. This maps to the MIDI output and MIDI channel used.
You can add or modify instruments by clicking the gear icon at the top right of the screen.


## Timing controls 

To the right of the main controls, with a purple background, are the timing controls.

<img src="images/patternEditorControls.png" alt="The shared pattern editor controls" width="800">

The first two controls define the time signature.

Patterns have their own time signatures, and these are independent of those shown in the Song Editor. 
As a result it is possible to play patterns with different time signatures against one another, 
both in song mode and edit mode. In the Song Editor the beginning of patterns are marked with ticks in 
the pattern blocks.

The time signature denominator has three different '8' options, same as the Song Editor. 
These different versions determine how the beat is defined. See [Tempo and Beats](11-tempo-and-beats.md) for details.

TODO: check this

The next control determines the number of bars in the pattern.

The next control declares how many parts to divide each beat into. This determines the granularity to to use when 
adding and resizing notes. It is possible to go free-form by deselecting the 'Snap' control.
  
