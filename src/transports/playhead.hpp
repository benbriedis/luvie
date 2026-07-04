#ifndef PLAYHEAD_HPP
#define PLAYHEAD_HPP

#include "itransport.hpp"
#include "observableSong.hpp"
#include "activePatternSet.hpp"
#include <FL/Fl_Widget.H>
#include <functional>
#include <string>
#include <vector>

class Port;
class PortRegistry;

// Routing for an instrument's soft (non-Jack) output: which port + 0-based channel.
struct MidiInstrRoute {
    std::string portName;
    int         channel0 = 0;
};

class Playhead : public ITimelineObserver {
	ITransport*         transport    = nullptr;
	ObservableSong* obsTl        = nullptr;
	ActivePatternSet*   aps          = nullptr;
	int                 numCols;
	int                 colWidth;
	Fl_Widget*          owner        = nullptr;
	int                 patternTrack = -1;  // >= 0: beat-relative view of that track

	bool  verbose      = false;
	bool  loopActive   = false;
	float lastPosition = 0.0f;
	std::function<bool(int)> loopEnabledFn;

	// ── Soft (Native/Debug) output, driven from the same crossing logic ──────────
	PortRegistry* portReg     = nullptr;
	bool          anySoftPort = false;   // are any Native/Debug ports present?
	bool          anyJackPort = false;   // are any Jack ports present?
	bool          jackClock   = false;   // is the Jack RT engine the active clock?
	bool          wasPlaying  = false;
	std::function<int(int,int,int)>         rowToMidi;   // (row, root, chord) → MIDI pitch
	std::function<MidiInstrRoute(int)>      instrRoute;  // instrument id → port/channel
	struct SoftActiveNote { std::string portName; int channel; int pitch; float offBar; };
	std::vector<SoftActiveNote> softNotes;

	// A port must be soft-sequenced unless the Jack RT engine is driving it (which
	// only happens for Jack ports while the Jack clock is active).
	bool portNeedsSoftSeq(const Port* p) const;

	void emitSoftNoteOn(int instrumentId, int midi, float velocity,
	                    float lenBeats, float beatsPerBar, float onBar);
	void emitSoftParam (int instrumentId, int ccNumber, int value);
	void flushSoftNoteOffs(float curPos);
	void allSoftNotesOff();

	static void timerCb(void* data);
	void tick();
	bool isInPattern(float bars) const;
	void checkVerboseNotes(float prevPos, float curPos);
	void checkLoopVerboseNotes(float prevPos, float curPos);
	void checkVerboseSongParams(float prevPos, float curPos);

	int       barsToPixel(float bars) const;
	float     pixelToBars(int px)     const;
	Fl_Color  currentHeadColor()      const;

public:
	std::function<void()>        onEndReached;
	std::function<void()>        onTick;
	std::function<std::string(int)> pitchName;   // optional: pitch index → "E4" etc.

	Playhead(int numCols, int colWidth);
	~Playhead();

	void setTransport(ITransport* t, ObservableSong* tl);
	void setActivePatterns(ActivePatternSet* a) { aps = a; }
	void setOwner(Fl_Widget* w)   { owner   = w; }
	void setVerbose(bool v)       { verbose = v; }
	void setPortRegistry(PortRegistry* r) { portReg = r; }
	void setHasSoftPorts(bool b)          { anySoftPort = b; }
	void setHasJackPorts(bool b)          { anyJackPort = b; }
	void setJackClockActive(bool b)       { jackClock   = b; }
	void setSoftRouting(std::function<int(int,int,int)> r2m,
	                    std::function<MidiInstrRoute(int)> ir) {
		rowToMidi  = std::move(r2m);
		instrRoute = std::move(ir);
	}
	// Release all held soft notes; call before the Port set is reconciled so notes
	// don't hang when a port is destroyed or changes backend.
	void panicSoftNotes() { allSoftNotesOff(); }
	void setLoopActive(bool a, std::function<bool(int)> enabledFn = nullptr);
	void setPatternTrack(int track) { patternTrack = track; }
	void setNumCols(int n)        { numCols = n; }

	void onTimelineChanged() override;

	void  drawTriangle(int rulerX, int rulerY, int rulerH);
	void  drawLine(int gridX, int gridY, int gridH);
	void  seek(int mouseX, int rulerX);
	int   xOffset() const;
	float currentBar() const;
};

#endif
