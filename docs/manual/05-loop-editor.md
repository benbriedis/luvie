# Loops

[← The song editor](04-song-editor.md) · [Contents](README.md) · [The pattern editors →](06-pattern-editors.md)

As well as the Song Editor Luvie comes with a Loop Editor. The Loop Editor show what is playing when Luvie is in 'Loop mode' and provides a more live experience that 'Song mode' - see Basics (TODO add link to Basics). When in Loop Mode it is easy to experiment with combining patterns in different ways.

<img src="images/loopEditor.png" alt="The Luvie loop editor" width="800">

In the screenshot above each column contains the patterns that currently exist for an instrument. Press the 'Flip' button to swap the columns for rows.

Any cells that are highlighted are patterns that are currently playing.

TODO: what a loop is, and how it differs from a pattern instance in the song.

## The loop editor

Left click on a pattern to start or stop playing it. Patterns will continue to play in loops until disabled.

Right click on a pattern block to open a pattern in the corresponding Pattern Editor, or to add, remove, or copy a pattern.

It is possible to rename an instrument in the editor by double clicking on the instrument name.

## Switching between modes

It is possible to switch between Song mode and Loop mode while music is playing. 
If music is playing then Luvie seeks to coordinate the playhead positions between the two modes 
so that play is uninterrupted. 

Plays out part bar...

How does this work if there are different time signatures playing and cf BPM.

NB the special mode.


TODO: switching modes is designed not to interrupt anything — loops keep playing
across the switch, the song playhead freezes (and greys) while in loop mode, and
returning to song mode seeks back to a bar boundary. Worth documenting because
the button changes colour to signal the pending seek.

NB grey playheads
