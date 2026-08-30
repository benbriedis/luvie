# The Basics

[← Configuration](01-configuration.md) · [Contents](README.md) · [MIDI output and instruments →](03-outputs.md)

<img src="images/songEditor.png" alt="Luvie song editor" width="1000">

## Songs, Loops and Patterns

Luvie can play in one of two modes: Song Mode and Loop Mode.
The Song/Loop mode toggle button is found on the top left of the window.

When in Song Mode a "song" is played from start to end. 
A song consists of multiple patterns that are turned on and off at strategic moments
so as to produce auditory bliss, or other.

Loop Mode has access to the same pool of patterns as Song Mode but in this case the patterns
are manually enabled and disabled by the user.

Loop Mode is more of a "live" experience whereas "Song Mode" is more composed or pre-baked.

Near the top of the window there are three tabs: Song Editor, Loop Editor and Pattern Editor.
The Song Editor displays what is played when in Song Mode and
the Loop Editor displays what is playing when in Loop Mode.

## The Pattern Editors

There are three different types of pattern editor: the harmony editor,
the pianoroll editor and the drum editor. 

There is only one Pattern Editor tab. The type of pattern editor shown
depends on which pattern is selected when you open the Pattern Editor.
Alternatively it's possible to open the desired Pattern Editor by right clicking on a
pattern name or pattern block when in the Song Editor, or right clicking on a
pattern block when in the Loop Editor.

## Context Popups

Luvie make extensive use of context menus to organise its operations.
You display these popup menus by right-clicking on appropriate items.
Luvie indicates that a popup menu by displaying a special cursor when you
hover the cursor over a section of the window that has a context menu available.
When in doubt about how to do something try right clicking on it.

TODO include image of the cursor

## The Settings Menu

Project-wide settings are to be found in the Settings Menu. To display it click on the
gear icon at the top right of the window.

## Instruments and Outputs


## The Song Editor 

## The Control Bar

## The Transport Bar

TODO: pattern editor tab, song editor tab, the transport bar along the bottom.
TODO: play/stop, etc


## Song mode vs Loop mode

There's a button on the top left that says 'Song'. 
Try clicking it....
It says 'Loop', it says 'Song', it says 'Loop', it says 'Song'.
Green, blue, green, blue, green, ...
You get the picture - there are two modes: 'Loop' and 'Song'.

The Song Editor shows what happens when in Song mode and
the Loop Editor shows what happen when in Loop mode.

[MOVE TO 'loop' chapter?]

It is possible to switch between the two modes in a fairly graceful fashion.
When swapping from Song mode to Loop mode the last playing patterns keep on playing.

When swapping from Loop mode to Song mode the currently playing bar will finish playing
and then the Song editor will take over. Note that if you have enabled any patterns when 
in Loop editor they will keep on playing until either you or the Song Editor turns them off again.

## The transport setting
TODO

## Where to go next

- [The song editor](04-song-editor.md) — arranging patterns into a song
- [The pattern editors](06-pattern-editors.md) — writing the patterns themselves
- [MIDI output and instruments](03-outputs.md) — getting sound out of Luvie
