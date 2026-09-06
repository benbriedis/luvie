// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef SIMPLETRANSPORT_HPP
#define SIMPLETRANSPORT_HPP

#include "itransport.hpp"
#include "observableSong.hpp"
#include <chrono>

class SimpleTransport : public ITransport {
	ObservableSong* timeline         = nullptr;
	float  savedPositionBars             = 0.0f;
	double playStartSeconds              = 0.0;   // barToSeconds(savedPositionBars) at play/seek time
	bool   playing                       = false;
	std::chrono::steady_clock::time_point playStart;

	// Loop -> Song hand-off, armed by endLoopMode() and landed by position() on the
	// way past handoffAtSecs. See the note there.
	bool   handoffArmed     = false;
	double handoffAtSecs    = 0.0;   // clock seconds the switch lands on (a bar line)
	float  handoffResume    = 0.0f;  // song bar playback continues from
	double handoffResumeSecs = 0.0;  // that bar in the song's own tempo map

	// Seconds elapsed on the clock, before any hand-off shift.
	double clockSeconds() const;
	// Position straight off the clock, before any hand-off shift.
	float rawPosition() const;

public:
	void setTimeline(ObservableSong* tl) { timeline = tl; }

	void  play()            override;
	void  pause()           override;
	void  rewind()          override;
	void  seek(float bars)  override;

	void  setLoopMode(bool loopMode) override;
	void  endLoopMode(float bars)    override;

	float position()  const override;
	bool  isPlaying() const override { return playing; }

	/* Snap to host transport position (called from LV2 port_event). */
	void syncFromHost(float bars, bool hostPlaying);
};

#endif
