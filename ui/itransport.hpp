#ifndef ITRANSPORT_HPP
#define ITRANSPORT_HPP

class ITransport {
public:
	virtual ~ITransport() = default;

	virtual void play()               = 0;
	virtual void pause()              = 0;
	virtual void rewind()             = 0;
	virtual void  seek(float bars)    = 0;

	virtual float position()  const  = 0;  // bars from start (float, e.g. 3.5 = bar 4 beat 3 of 4)
	virtual bool   isPlaying() const = 0;
};

#endif
