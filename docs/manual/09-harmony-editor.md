# Harmony patterns

[← The pattern editors](05-pattern-editors.md) · [Contents](README.md) · [Loops →](07-loops.md)

A harmony pattern does not store fixed pitches. It stores positions within a
chord or scale, and the pitches come out of how that is currently interpreted.
Change the interpretation and the same pattern plays different notes.

TODO: screenshot of the harmony editor with the purple control panel visible.

## Pitch interpretation

The purple control panel holds the current interpretation of the pattern's
pitches. Changing it does not modify the pattern — it changes the pitches that
are output.

Contrast this with the Transpose option on the note context menu, which changes
the pattern itself.

TODO: enumerate the controls on the panel and what each one does.

## Pitch groups

A pitch group is a set of rows covering one chord or scale. Above it sits the
next group, a real octave higher.

TODO: chords, scales and multiple octaves — how many rows each produces.

Because a group steps up by a full octave rather than by the span of the chord,
extended chords can overlap into the group above. Pitches are therefore not
always strictly increasing as you move up the rows. This is deliberate: it keeps
the root of each group octave-stable as you change chord or scale.

## Bonus notes

Bonus notes sit in bonus rows, outside the ordinary pitch groups.

TODO: how a bonus note is created.

TODO: describe how bonus notes get stretched up additional octaves when using
smaller pitch groups.

A bonus row disappears when every note in it, across all octaves, is removed.
