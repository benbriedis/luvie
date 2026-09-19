// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef PLAYHEAD_HPP
#define PLAYHEAD_HPP

#include "itransport.hpp"
#include "observableSong.hpp"
#include "loopManager.hpp"
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
	LoopManager*   loopMgr          = nullptr;
	int                 numCols;
	int                 colWidth;
	Fl_Widget*          owner        = nullptr;
	int                 patternTrack = -1;  // >= 0: beat-relative view of that track

	bool  verbose      = false;
	bool  loopActive   = false;
	// Song view head state. Live: tracks the transport, full colour. Frozen: greyed
	// and pinned at frozenBar (Loop mode). Handoff: greyed but moving again, tracking
	// the transport shifted by headOffset — the Loop -> Song run-up, which lands the
	// head on the resume bar exactly as the engine switches.
	enum class SongHead { Live, Frozen, Handoff };
	SongHead songHead  = SongHead::Live;
	float frozenBar    = 0.0f;
	float headOffset   = 0.0f;
	float lastPosition = 0.0f;
	std::function<bool(int)> loopEnabledFn;

	// Phase for a displayed pattern that nothing is playing — not switched on in the
	// Loop Editor and not under a song block. Nothing is sounding it, so the editor
	// owns where it sits: it tiles the song from this bar. Recording needs a real
	// place to put a note whether or not the pattern is switched on, and the drawn
	// head has to agree with it. Held for one pattern at a time and read back only
	// for that one (freeAnchorFor), so a pattern never inherits the phase that
	// flexible-bars recording left on another.
	float freeAnchorBar   = 0.0f;
	int   freeAnchorPatId = 0;
	float freeAnchorFor(int patId) const;

	// ── Soft (Native/Debug) output, driven from the same crossing logic ──────────
	PortRegistry* portReg     = nullptr;
	bool          anySoftPort = false;   // are any Native/Debug ports present?
	bool          anyJackPort = false;   // are any Jack ports present?
	bool          jackClock   = false;   // is the Jack RT engine the active clock?
	bool          wasPlaying  = false;
	std::function<MidiInstrRoute(int)>      instrRoute;  // instrument id → port/channel
	struct SoftActiveNote { std::string portName; int channel; int pitch; float offBar; };
	std::vector<SoftActiveNote> softNotes;
	// Pending skipNoteOnce() entries for the soft output. See ITransport::skipNoteOnce.
	struct SoftSkip { int instrumentId; int pitch; float bar; float tol; };
	std::vector<SoftSkip> softSkips;

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
	std::string noteLabel(const Pattern& pat, const Note& note, int chordIndex) const;
	void checkLoopVerboseNotes(float prevPos, float curPos);
	void checkVerboseSongParams(float prevPos, float curPos);

	int       barsToPixel(float bars) const;
	float     pixelToBars(int px)     const;
	Fl_Color  currentHeadColor()      const;
	int       displayedPatternId()    const;  // pattern the editor shows for patternTrack

	// The transport's position. Already folded into the song-loop region when one is
	// armed — the clock that sequences owns the wrap (see ITransport::setSongLoop),
	// so there is nothing to fold here.
	float     livePosition()          const;
	// Where the head is drawn: livePosition(), except in the song view's Frozen and
	// Handoff states. Only for display — note emission always uses livePosition().
	float     displayBars()           const;

public:
	std::function<void()>        onEndReached;
	std::function<void()>        onTick;
	std::function<std::string(int)> pitchName;   // optional: pitch index → "E4" etc.

	// Song-loop query (song mode only): fills [startBar, endBar) in transport-bar
	// units and returns true when the transport loop toggle is on. The transport wraps
	// playback itself; the playhead only needs to know a loop is armed, so it does not
	// seek a position that legitimately sits past the end of the grid.
	std::function<bool(float& startBar, float& endBar)> songLoopRange;

	Playhead(int numCols, int colWidth);
	~Playhead();

	void setTransport(ITransport* t, ObservableSong* tl);
	void setLoopManager(LoopManager* a) { loopMgr = a; }
	void setOwner(Fl_Widget* w)   { owner   = w; }
	void setVerbose(bool v)       { verbose = v; }
	void setPortRegistry(PortRegistry* r) { portReg = r; }
	void setHasSoftPorts(bool b)          { anySoftPort = b; }
	void setHasJackPorts(bool b)          { anyJackPort = b; }
	void setJackClockActive(bool b)       { jackClock   = b; }
	void setSoftRouting(std::function<MidiInstrRoute(int)> ir) {
		instrRoute = std::move(ir);
	}
	// Release all held soft notes; call before the Port set is reconciled so notes
	// don't hang when a port is destroyed or changes backend.
	void panicSoftNotes() { allSoftNotesOff(); }
	// Soft-output counterpart of ITransport::skipNoteOnce: drop the one firing of
	// (instrument, pitch) within `tol` bars of `bar`. instrumentId 0 matches any.
	void skipNoteOnce(int instrumentId, int midi, float bar, float tol) {
		softSkips.push_back({instrumentId, midi, bar, tol});
	}
	void setLoopActive(bool a, std::function<bool(int)> enabledFn = nullptr);
	bool isLoopActive() const { return loopActive; }
	// Song view only: freeze the playhead greyed at `bar` (Song→Loop) instead of
	// tracking the transport; clear to resume tracking (Loop→Song).
	void setFrozen(bool f, float bar = 0.0f);
	// Song view only: Loop → Song hand-off. The head tracks the transport again but
	// stays greyed, displaced by `offsetBars` so it runs up to the resume bar and
	// gets there just as the engine switches. Clearing it returns to Live.
	void setHandoff(bool on, float offsetBars = 0.0f);
	void setPatternTrack(int track) { patternTrack = track; }

	// Everything the displayed pattern's phase depends on, sampled together. Growth
	// (flexible-bars recording) needs the anchor and the beat scale as well as the
	// position, and reading them through separate calls would let them come from
	// different instants — the one thing re-phasing must not do.
	struct PatternPos {
		bool  running      = false;  // false: there is no pattern; nothing below is set
		float elapsedBeats = 0.0f;   // beats since the anchor, NOT folded by the length
		float beatsPerBar  = 0.0f;   // pattern beats per SONG bar (not per pattern bar)
		float anchorBar    = 0.0f;   // song bar where pattern beat 0 falls
		bool  manualLoop   = false;  // anchored by a Loop-Editor switch, not a song block
		// Nothing is playing this pattern, so the anchor above is the editor's own
		// (see freeAnchorBar) rather than anything the engine is following. Moving it
		// is therefore free: there is no playback to fall out of step with.
		bool  free         = false;
	};
	PatternPos patternPos(float bars) const;
	// Re-anchor the displayed pattern: the LoopManager anchor when something is
	// playing it from there (Loop Mode, or a manual loop layered over Song Mode), the
	// editor's own free anchor when nothing is playing it at all. Not for a pattern
	// running from a song block — there the engine takes its phase from the block, so
	// moving this alone would split the UI from what is heard. PatternPos says which
	// case applies.
	void reanchorPattern(float anchorBar);
	// Take the phase the displayed pattern has right now as its free anchor, so that
	// if something stops playing it part-way through — the song block under the
	// playhead ends — the position carries on from where it was instead of snapping
	// to the default tiling. Called when a take opens.
	void pinPatternAnchor();

	// Pattern-local beat for a transport position, or -1 when the pattern is not
	// running (see the definition). Recording asks this where to put a note, so it
	// and the drawn playhead can never disagree.
	float patternBeat(float bars) const;
	// The live transport position, as patternBeat() wants it.
	float transportBars() const { return livePosition(); }
	// Is the transport rolling? Recording asks before writing anything.
	bool  transportPlaying() const { return transport && transport->isPlaying(); }
	void setNumCols(int n)        { numCols = n; }
	void setColWidth(int cw)      { colWidth = cw; }

	void onTimelineChanged() override;

	void  drawTriangle(int rulerX, int rulerY, int rulerH);
	void  drawLine(int gridX, int gridY, int gridH);
	void  seek(int mouseX, int rulerX);
	int   xOffset() const;
	float currentBar() const;
};

#endif
