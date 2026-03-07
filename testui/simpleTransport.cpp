#include "simpleTransport.hpp"

void SimpleTransport::play() {
	if (!playing) {
		playStart = std::chrono::steady_clock::now();
		playing   = true;
	}
}

void SimpleTransport::pause() {
	if (playing) {
		savedPosition = position();
		playing       = false;
	}
}

void SimpleTransport::stop() {
	pause();
}

void SimpleTransport::rewind() {
	playing       = false;
	savedPosition = 0.0;
}

double SimpleTransport::position() const {
	if (!playing) return savedPosition;
	auto now = std::chrono::steady_clock::now();
	return savedPosition + std::chrono::duration<double>(now - playStart).count();
}
