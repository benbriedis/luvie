#include "simpleTransport.hpp"

void SimpleTransport::play() {
	if (!playing_) {
		playStart_ = std::chrono::steady_clock::now();
		playing_   = true;
	}
}

void SimpleTransport::pause() {
	if (playing_) {
		savedPosition_ = position();
		playing_       = false;
	}
}

void SimpleTransport::stop() {
	pause();
}

void SimpleTransport::rewind() {
	playing_       = false;
	savedPosition_ = 0.0;
}

double SimpleTransport::position() const {
	if (!playing_) return savedPosition_;
	auto now = std::chrono::steady_clock::now();
	return savedPosition_ + std::chrono::duration<double>(now - playStart_).count();
}
