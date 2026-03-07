#ifndef SIMPLETRANSPORT_HPP
#define SIMPLETRANSPORT_HPP

#include "itransport.hpp"
#include <chrono>

class SimpleTransport : public ITransport {
	double savedPosition_ = 0.0;
	bool   playing_       = false;
	std::chrono::steady_clock::time_point playStart_;

public:
	void   play()   override;
	void   pause()  override;
	void   stop()   override;
	void   rewind() override;

	double position()  const override;
	bool   isPlaying() const override { return playing_; }
};

#endif
