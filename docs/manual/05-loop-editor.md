# Loops

[← The song editor](04-song-editor.md) · [Contents](README.md) · [The pattern editors →](06-pattern-editors.md)

As well as the Song Editor Luvie comes with a Loop Editor. The Loop Editor show what is playing when Luvie is in 'Loop mode' and provides a more live experience that 'Song mode' - see Basics (TODO add link to Basics). When in Loop Mode it is easy to experiment with combining patterns in different ways.

<img src="images/loopEditor.png" alt="The Luvie loop editor" width="800">

In the screenshot above each column contains the patterns that currently exist for an instrument. Press the 'Flip' button to swap the columns for rows.

Any cells that are highlighted are patterns that are currently playing.

TODO: what a loop is, and how it differs from a pattern instance in the song.

## Song mode and loop mode

TODO: what each mode plays, and how to switch.

TODO: switching modes is designed not to interrupt anything — loops keep playing
across the switch, the song playhead freezes (and greys) while in loop mode, and
returning to song mode seeks back to a bar boundary. Worth documenting because
the button changes colour to signal the pending seek.

## The loop editor

TODO: creating, naming and deleting loops; the loop ruler; the context menu.

TODO: the loop panel is BPM-only — it has no time signature of its own, because
each pattern brings its own. Cross-reference
[BPM and time signatures](09-beats-and-times.md).
