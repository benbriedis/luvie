# BPM and time signatures

[← The drum pattern editor](08-drum-pattern-editor.md) · [Contents](README.md)

## BPM has two meanings

JACK (and many DAWs) arguably misuse the term BPM. What they really mean is crotchets per minute. 

Luvie needs to handle other definitions of a beat — using dotted crotchet as a beat
for instance — so in Luvie, BPM means actually does mean beats per minute. 
One implication is that when the song editor uses a non-crotchet beat, JACK and Luvie  
show different numbers for the same tempo. 

## Time signatures: eighth variants

The Song Editor and the pattern editors both have time signature controls.
There are three definitions for eights:
- 8
- 8 (dotted quaver)
- 8 (dotted crotchet)

The dotted quaver and the dotted crotchet indicate how the beat is to be defined: is
the beat a plain crotchet, a dotted quaver, or a dotted crotchet?
This is useful, for example, when changing from 4/4 to 6/8. Is the 6/8 bar meant to equal
the full length of the 4/4 bar, or half the length? 
In the former case use 8 (dotted crotchet), in the latter use 8 (dotted quaver).

## Patterns have their own time signatures

So... the Song Editor has its time signatures, set on a ruler at the top of the window, and
each pattern has its own independent time signature. Awkward, but that's loopers for you.
It's up to the user to ensure the time signatures of the patterns match one another as well as 
those in the song editor. 

In some cases you may wish to experiment with playing patterns of different lengths
and time signatures against one another. It might sound super-cool or like a horrible mess or both.


