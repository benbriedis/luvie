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

It is possible to switch between the two modes in a fairly graceful fashion.
When swapping from Song mode to Loop mode the last playing patterns keep on playing.

When swapping from Loop mode to Song mode the currently playing bar will finish playing
and then the Song editor will take over. Note that if you have enabled any patterns when 
in Loop editor they will keep on playing until either you or the Song Editor turns them off again.

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

<img src="images/contextCursor.png" alt="The context menu cursor" width="60">

## The Transport 

The transport controls sit along the bottom of the window. They consist of a
loop/don't loop toggle button, a play/pause button, and a rewind button.

When in song mode rewind takes the playhead to the "start" marker and play 
ends at the end marker.
FIX THIS
If looping is enabled then the playhead loops been the "start" and "end" markers.

## Alerts

To the right of the transport controls there is an "alerts" icon. Hover over it to 
view any current alerts. The most common alerts relate to the Jack server not being present.

## The Settings Menu

Project-wide settings are to be found in the Settings Menu. To display it click on the
gear icon at the top right of the window.

## Export and import

So you started a project in Carla and now decide you want to move over to Ardour? Or maybe as a standalone?
This is what export and import is about. You can access these though the 'settings' menu - just click the
gear icon at the top right of the window.

You can also export projects to give you save multiple versions, save backups, etc, in some circumstances.

## Where to go next

- [The song editor](04-song-editor.md) — arranging patterns into a song
- [The pattern editors](06-pattern-editors.md) — writing the patterns themselves
- [MIDI output and instruments](03-outputs.md) — getting sound out of Luvie
