#ifndef SIMPLETRANSPORT_HPP
#define SIMPLETRANSPORT_HPP

#include "itransport.hpp"
#include <chrono>

class SimpleTransport : public ITransport {
	double savedPosition = 0.0;
	bool   playing       = false;
	std::chrono::steady_clock::time_point playStart;

public:
	void   play()               override;
	void   pause()              override;
	void   rewind()             override;
	void   seek(double seconds) override;

	double position()  const override;
	bool   isPlaying() const override { return playing; }
};

#endif
