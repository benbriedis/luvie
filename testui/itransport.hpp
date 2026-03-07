#ifndef ITRANSPORT_HPP
#define ITRANSPORT_HPP

class ITransport {
public:
	virtual ~ITransport() = default;

	virtual void play()    = 0;
	virtual void pause()   = 0;
	virtual void stop()    = 0;
	virtual void rewind()  = 0;

	virtual double position()  const = 0;  // seconds from start
	virtual bool   isPlaying() const = 0;
};

#endif
