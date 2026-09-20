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

	// The song editor's Start/End loop region, in absolute song bars with endBar
	// exclusive. A backend that sequences owns the wrap: it plays the region round,
	// and position() reports the wrapped bar — so nothing above has to fold a raw
	// position, and a paused playhead stays put while the markers are dragged. The
	// default ignores it (plugin mode ships the region to the DSP instead).
	virtual void setSongLoop(bool /*enabled*/, float /*startBar*/, float /*endBar*/) {}

	// Loop -> Song hand-off: leave loop mode and resume song playback at `bars` as a
	// *continuous* move. Backends must not relocate the host clock and must not reset
	// controllers — the loops' held notes are released, nothing more — and must apply
	// the position and the mode together, so no cycle renders one without the other.
	// The default (seek then flip) is right for clocks whose seek is already
	// glitch-free; JackTransport and the plugin override it.
	virtual void endLoopMode(float bars) { seek(bars); setLoopMode(false); }

	// Song -> Loop hand-off, the mirror of endLoopMode(): the song plays out to the
	// bar line at `atBar` and the loops come in there, as a *continuous* move.
	// Backends must not relocate the host clock and must not reset controllers. The
	// default is the old instant flip, which is right for a clock that does not
	// sequence and so has nothing to hand over.
	virtual void beginLoopMode(float /*atBar*/) { setLoopMode(true); }

	// Land a Loop-mode scene change on the bar line at `atBar`. The engine keeps
	// sounding the set it has until then, and swaps to LoopManager::pendingPatterns()
	// there. Backends must not relocate the clock, must not change the tempo map and
	// must not reset controllers: nothing moves but which patterns fire. The default
	// ignores it — a clock that does not sequence has no content to swap, and the
	// soft playback path lands the change from Playhead::tick instead.
	virtual void armScene(float /*atBar*/) {}

	// Recording has already sounded this note live, and quantising put it just ahead
	// of the playhead: drop the one firing of (instrument, pitch) within tolBars of
	// song bar `bar`, so it is not heard again moments later. Later passes play it as
	// normal. instrumentId 0 matches any instrument. The default ignores it (clocks
	// that do not sequence, and the plugin, where the DSP owns playback).
	virtual void skipNoteOnce(int /*instrumentId*/, int /*midiPitch*/,
	                          double /*bar*/, double /*tolBars*/) {}
};

#endif
