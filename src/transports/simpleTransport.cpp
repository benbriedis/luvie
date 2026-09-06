// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "simpleTransport.hpp"
#include <algorithm>
#include <cmath>

void SimpleTransport::play() {
	if (playing) return;
	playStartSeconds = timeline ? timeline->barToSeconds(savedPositionBars) : 0.0;
	playStart        = std::chrono::steady_clock::now();
	playing          = true;
	handoffArmed     = false;
}

void SimpleTransport::pause() {
	if (!playing) return;
	// A hand-off still waiting for its bar line completes when the clock stops:
	// there is no phase left to align to. Same rule the RT engine applies when a
	// cycle arrives with the transport no longer rolling.
	savedPositionBars = (handoffArmed && clockSeconds() < handoffAtSecs) ? handoffResume
	                                                                    : position();
	handoffArmed      = false;
	playing           = false;
}

void SimpleTransport::rewind() {
	savedPositionBars = 0.0f;
	handoffArmed      = false;
	if (playing) {
		playStartSeconds = 0.0;
		playStart        = std::chrono::steady_clock::now();
	}
}

void SimpleTransport::seek(float bars) {
	savedPositionBars = std::max(0.0f, bars);
	handoffArmed      = false;
	if (playing) {
		playStartSeconds = timeline ? timeline->barToSeconds(savedPositionBars) : 0.0;
		playStart        = std::chrono::steady_clock::now();
	}
}

void SimpleTransport::setLoopMode(bool /*loopMode*/) {
	// Entering a mode supersedes a hand-off out of one that has not landed yet — the
	// user clicked back into Loop Mode mid-transition. Mirrors Sequencer::setLoopMode.
	handoffArmed = false;
}

void SimpleTransport::endLoopMode(float bars) {
	// This clock has no RT engine to land the switch for it, so the wait lives here:
	// arm it, and let position() apply it on the way past the next bar line. Applying
	// it now instead would cut the loops off mid-bar and drop the run-up the song
	// playhead shows. Same landing rule as Sequencer::handoffPoint(), so the internal
	// and JACK clocks hand off at the same place.
	if (!playing || !timeline) { seek(bars); return; }
	// Both ends of the switch in seconds: where the loops stop (a bar line on the
	// frozen Loop-Mode map the clock is running on) and where the song picks up (that
	// bar in the song's own map, which is what will be read back once Loop Mode
	// ends). Resuming is then a shift of the song's timeline in time, not in bars, so
	// every marker still ahead keeps the position the user gave it.
	handoffAtSecs     = timeline->barToSeconds(std::ceil(rawPosition() - 1.0e-4f));
	handoffResume     = bars;
	handoffResumeSecs = timeline->songBarToSeconds(bars);
	handoffArmed      = true;
}

void SimpleTransport::syncFromHost(float bars, bool hostPlaying) {
	savedPositionBars = bars;
	handoffArmed      = false;
	if (hostPlaying) {
		playStartSeconds = timeline ? timeline->barToSeconds(bars) : 0.0;
		playStart        = std::chrono::steady_clock::now();
		playing          = true;
	} else {
		playing = false;
	}
}

double SimpleTransport::clockSeconds() const {
	auto now = std::chrono::steady_clock::now();
	return playStartSeconds + std::chrono::duration<double>(now - playStart).count();
}

float SimpleTransport::rawPosition() const {
	if (!playing || !timeline) return savedPositionBars;
	return (float)timeline->secondsToBar(clockSeconds());
}

float SimpleTransport::position() const {
	if (!playing || !timeline) return savedPositionBars;
	double secs = clockSeconds();
	// Past the point the hand-off was armed for: the loops are done, and the song's
	// timeline is slid along so its resume bar falls exactly there. The shift stays
	// applied for the rest of this play stretch — every call that re-anchors the
	// clock (play/seek/rewind/pause) disarms it first.
	if (handoffArmed && secs >= handoffAtSecs)
		secs = handoffResumeSecs + (secs - handoffAtSecs);
	return std::max(0.0f, (float)timeline->secondsToBar(secs));
}
