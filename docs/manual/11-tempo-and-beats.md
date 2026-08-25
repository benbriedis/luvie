# Tempo, beats and bars

[← The pattern editors](06-intro-pattern-editors.md) · [Contents](README.md) · [Loops →](08-loop-editor.md)

## BPM is not "crotchets per minute"

JACK, probably LV2, and most DAWs arguably misuse the term BPM. What they really
mean is crotchets per minute. That is convenient in some ways and unhelpful in
others, and the loss of accuracy is regrettable.

Luvie needs to handle other definitions of a beat — a beat of a dotted crotchet,
for instance — so in Luvie, BPM means beats per minute and a beat is whatever
the beat definition says it is. One implication is that when the song editor
uses a non-crotchet beat, JACK and Luvie will show different numbers for the
same tempo. Neither is wrong; they are counting different things.

## Beat definitions and time signatures in the song editor

These are used almost entirely within the song editor itself, to work out where
patterns start and stop and how long a bar is. Patterns look after their own
timing — see below.

The first values additionally serve as the defaults for newly created patterns.

The song editor's BPM, by contrast, *is* used by the patterns.

TODO: confirm the "defaults for new patterns" behaviour against the code before
this is published; the original note was uncertain about it.

## Patterns time themselves

A pattern is timed by its own time signature and beat definition, so the song's
settings cancel out. This is why a pattern keeps its shape when you place it in
a song whose time signature differs.

TODO: worked example — the same pattern under two different song time
signatures.

## Beat definition in the loop editor

The loop editor cannot really act as transport master, because patterns may each
define different time signatures. It cannot even hand JACK a BPM, since JACK
expects crotchets per minute and there can be several different ones in play at
once. Where a time master is wanted here, the song editor's settings are the
ones to use.

## Tempo changes

TODO: adding tempo markers in the song editor, and linear (ramped) tempo
changes.

TODO: changing the tempo while playing pins the playhead where it is and simply
changes the rate from that point — worth stating, because it is what makes
tempo changes usable live.
