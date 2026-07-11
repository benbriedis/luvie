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
};

#endif
