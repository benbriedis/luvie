#ifndef SIMPLETRANSPORT_HPP
#define SIMPLETRANSPORT_HPP

#include "itransport.hpp"
#include "observableTimeline.hpp"
#include <chrono>

class SimpleTransport : public ITransport {
	ObservableTimeline* timeline         = nullptr;
	float  savedPositionBars             = 0.0f;
	double playStartSeconds              = 0.0;   // barToSeconds(savedPositionBars) at play/seek time
	bool   playing                       = false;
	std::chrono::steady_clock::time_point playStart;

public:
	void setTimeline(ObservableTimeline* tl) { timeline = tl; }

	void  play()            override;
	void  pause()           override;
	void  rewind()          override;
	void  seek(float bars)  override;

	float position()  const override;
	bool  isPlaying() const override { return playing; }
};

#endif
