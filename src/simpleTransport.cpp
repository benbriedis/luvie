#include "simpleTransport.hpp"
#include <algorithm>

void SimpleTransport::play() {
	if (playing) return;
	playStartSeconds = timeline ? timeline->barToSeconds(savedPositionBars) : 0.0;
	playStart        = std::chrono::steady_clock::now();
	playing          = true;
}

void SimpleTransport::pause() {
	if (!playing) return;
	savedPositionBars = position();
	playing           = false;
}

void SimpleTransport::rewind() {
	savedPositionBars = 0.0f;
	if (playing) {
		playStartSeconds = 0.0;
		playStart        = std::chrono::steady_clock::now();
	}
}

void SimpleTransport::seek(float bars) {
	savedPositionBars = std::max(0.0f, bars);
	if (playing) {
		playStartSeconds = timeline ? timeline->barToSeconds(savedPositionBars) : 0.0;
		playStart        = std::chrono::steady_clock::now();
	}
}

float SimpleTransport::position() const {
	if (!playing) return savedPositionBars;
	auto   now     = std::chrono::steady_clock::now();
	double elapsed = std::chrono::duration<double>(now - playStart).count();
	if (timeline)
		return (float)timeline->secondsToBar(playStartSeconds + elapsed);
	return savedPositionBars;
}
