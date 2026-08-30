// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef ITRANSPORT_HPP
#define ITRANSPORT_HPP

#include <functional>

class ITransport {
public:
	virtual ~ITransport() = default;

	virtual void play()               = 0;
	virtual void pause()              = 0;
	virtual void rewind()             = 0;
	virtual void  seek(float bars)    = 0;

	// Re-place the playhead at `bars` as a *continuous* move, not a user jump:
	// used to keep the current musical position pinned across a tempo-map change
	// so playback stays smooth. Unlike seek(), backends must not silence/re-trigger
	// notes for this reposition. Default is a plain seek() (fine for clocks whose
	// seek is already glitch-free); JACK overrides it to avoid a note reset.
	virtual void reanchor(float bars) { seek(bars); }

	virtual float position()  const  = 0;  // bars from start (float, e.g. 3.5 = bar 4 beat 3 of 4)
	virtual bool   isPlaying() const = 0;

	// Loop mode: when active, generate MIDI only for enabled patterns, looping indefinitely.
	virtual void setLoopMode(bool /*loopMode*/) {}

	// Loop -> Song hand-off: leave loop mode and resume song playback at `bars` as a
	// *continuous* move. Backends must not relocate the host clock and must not reset
	// controllers — the loops' held notes are released, nothing more — and must apply
	// the position and the mode together, so no cycle renders one without the other.
	// The default (seek then flip) is right for clocks whose seek is already
	// glitch-free; JackTransport and the plugin override it.
	virtual void endLoopMode(float bars) { seek(bars); setLoopMode(false); }
};

#endif
