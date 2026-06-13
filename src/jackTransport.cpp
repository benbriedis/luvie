#include "jackTransport.hpp"
#include "chords.hpp"
#include "paramLaneTypes.hpp"
#include <FL/Fl.H>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

// ── Construction / destruction ────────────────────────────────────────────────

JackTransport::JackTransport() {}

JackTransport::~JackTransport()
{
    if (aps)      aps->removeObserver(this);
    if (timeline) timeline->removeObserver(this);
    if (client && jackAlive.load()) {
        jack_deactivate(client);
        for (auto& [name, port] : midiPorts_)
            jack_port_unregister(client, port);
        jack_client_close(client);
    }
}

// ── Open ──────────────────────────────────────────────────────────────────────

static void jackSilentLog(const char*) {}

void JackTransport::silenceLogging()
{
    // Route JACK's own console logging to a no-op so the once-per-second
    // availability poll doesn't spam the terminal with "cannot connect to
    // server" messages. Call once before opening; skipped in verbose mode.
    jack_set_error_function(jackSilentLog);
    jack_set_info_function(jackSilentLog);
}

bool JackTransport::open(const char* clientName, bool enableMidi)
{
    midiEnabled = enableMidi;

    // JackNoStartServer: connect only to an already-running server. Luvie waits
    // (polls) for an external JACK server rather than spawning one itself.
    client = jack_client_open(clientName, JackNoStartServer, nullptr);
    if (!client)
        return false;

    sampleRate = jack_get_sample_rate(client);

    jack_set_process_callback(client, processCallback, this);

    // Detect the server going away without polling: JACK invokes this from one
    // of its own threads, so the thunk only flips an atomic and hands off to the
    // main thread via Fl::awake.
    jack_on_shutdown(client, &JackTransport::shutdownThunk, this);

    if (jack_activate(client) != 0) {
        fprintf(stderr, "JackTransport: could not activate JACK client\n");
        jack_client_close(client);
        client = nullptr;
        return false;
    }

    jackAlive.store(true);
    return true;
}

// ── Shutdown handling ─────────────────────────────────────────────────────────

void JackTransport::shutdownThunk(void* arg)
{
    // Runs on a JACK-internal thread. The client is already dead here, so we
    // must not call any JACK functions — just mark the transport down and wake
    // the main thread to run the (UI-thread) onShutdown handler.
    auto* self = static_cast<JackTransport*>(arg);
    self->jackAlive.store(false);
    self->playing_.store(false);
    Fl::awake([](void* a) {
        auto* s = static_cast<JackTransport*>(a);
        if (s->onShutdown) s->onShutdown();
    }, self);
}

void JackTransport::close()
{
    // Reached after the server has shut down (client is a zombie) or while
    // tearing down for reconnect. Free the client handle — JACK requires
    // jack_client_close() even on a dead client — and drop the MIDI ports,
    // whose handles belonged to the old client and are now invalid. The fresh
    // client created by the next open() re-registers them.
    if (client) {
        jack_client_close(client);
        client = nullptr;
    }
    {
        std::lock_guard<std::mutex> lk(portsMutex);
        midiPorts_.clear();
    }
    jackAlive.store(false);

    // Reset RT-thread state so a reconnected client starts clean.
    activeNotes.clear();
    lastFrame  = 0;
    wasPlaying = false;
    firstCall  = true;
}

// ── UI-thread setters ─────────────────────────────────────────────────────────

void JackTransport::setTimeline(ObservableSong* tl)
{
    if (timeline) timeline->removeObserver(this);
    timeline = tl;
    if (timeline) timeline->addObserver(this);
    rebuildSnapshot();
}

void JackTransport::setNoteParams(int root, int chord)
{
    rootPitch = root;
    chordType = chord;
    rebuildSnapshot();
}

void JackTransport::setInstruments(const std::vector<InstrumentRouting>& routings)
{
    instrumentMap_.clear();
    for (const auto& r : routings)
        instrumentMap_[r.instrumentId] = r;
    rebuildSnapshot();
}

void JackTransport::enqueue(const std::string& portName, const uint8_t* data, int len)
{
    if (!client || !midiEnabled || len < 1 || len > 3) return;
    PendingMsg m{portName, {0, 0, 0}, len};
    for (int i = 0; i < len; i++) m.data[i] = data[i];
    std::lock_guard<std::mutex> lk(pendingMutex_);
    pendingMsgs_.push_back(std::move(m));
}

void JackTransport::sendProgramChange(const std::string& portName, int midiCh0,
                                      int bankMsb, int bankLsb, int program)
{
    uint8_t ch = static_cast<uint8_t>(midiCh0 & 0x0F);
    if (bankMsb >= 0) {
        uint8_t m[3] = {static_cast<uint8_t>(0xB0 | ch), 0,  static_cast<uint8_t>(bankMsb)};
        enqueue(portName, m, 3);
    }
    if (bankLsb >= 0) {
        uint8_t m[3] = {static_cast<uint8_t>(0xB0 | ch), 32, static_cast<uint8_t>(bankLsb)};
        enqueue(portName, m, 3);
    }
    if (program >= 0) {
        uint8_t m[2] = {static_cast<uint8_t>(0xC0 | ch), static_cast<uint8_t>(program)};
        enqueue(portName, m, 2);
    }
}

void JackTransport::setActivePatterns(ActivePatternSet* a)
{
    if (aps) aps->removeObserver(this);
    aps = a;
    if (aps) aps->addObserver(this);
    rebuildSnapshot();
}

void JackTransport::setLoopMode(bool mode)
{
    loopMode = mode;
    rebuildSnapshot();
}

// ── Snapshot building (UI thread) ─────────────────────────────────────────────

static constexpr float drumNoteLen = 0.1f;

void JackTransport::rebuildSnapshot()
{
    if (!timeline) return;

    const Timeline& tl = timeline->get();
    Snapshot newSnap;

    // Build time segments at each BPM / time-signature boundary.
    {
        std::set<int> breakpoints;
        breakpoints.insert(0);
        for (auto& m : tl.bpms)     breakpoints.insert(m.bar);
        for (auto& m : tl.timeSigs) breakpoints.insert(m.bar);

        double accSecs = 0.0;
        int    prevBar = 0;
        float  prevBpm = timeline->bpmAt(0);
        int    prevTop, prevBot;
        timeline->timeSigAt(0, prevTop, prevBot);

        for (int bar : breakpoints) {
            if (bar != 0) {
                double secsPerBar = prevTop * 60.0 / prevBpm;
                accSecs += (bar - prevBar) * secsPerBar;
            }

            int top, bot;
            timeline->timeSigAt(bar, top, bot);
            float bpm = timeline->bpmAt(bar);

            newSnap.segs.push_back({(float)bar, bpm, top, accSecs});

            prevBar = bar;
            prevBpm = bpm;
            prevTop = top;
        }
    }

    // Build per-track note data.
    auto buildNotes = [&](InstanceSnap& is, const Pattern* pat, int trackIdx) {
        is.portName    = "";
        is.midiChannel = trackIdx % 16;
        if (pat->instrumentId != 0) {
            auto it = instrumentMap_.find(pat->instrumentId);
            if (it != instrumentMap_.end()) {
                is.portName    = it->second.portName;
                is.midiChannel = it->second.midiChannel - 1;
            }
        }
        if (pat->type == PatternType::DRUM) {
            bool anySolo = !pat->drumSolo.empty();
            for (const DrumNote& dn : pat->drumNotes) {
                bool isSolo = pat->drumSolo.count(dn.note) > 0;
                bool isMute = pat->drumMute.count(dn.note) > 0;
                if (isMute || (anySolo && !isSolo)) continue;
                int midi = std::clamp(dn.note, 0, 127);
                is.notes.push_back({midi, dn.beat, drumNoteLen, dn.velocity});
            }
        } else {
            for (const Note& note : pat->notes) {
                if (note.disabled) continue;
                int midi = rowToMidi(note.row, rootPitch, chordType);
                is.notes.push_back({midi, note.beat, note.length, note.velocity});
            }
        }
    };

    // Pre-compute all firing events (point values + half-integer crossings) for a lane.
    auto buildParamEvents = [](const ParamLane& lane) -> std::vector<ParamEventSnap> {
        std::vector<ParamEventSnap> evts;
        auto sink = [&](float beat, int value) { evts.push_back({beat, value}); };
        for (int i = 0; i < (int)lane.points.size(); i++) {
            evts.push_back({lane.points[i].beat, lane.points[i].value});
            if (i + 1 < (int)lane.points.size())
                densifyParamRamp(lane.points[i].beat,  lane.points[i+1].beat,
                                 lane.points[i].value, lane.points[i+1].value, sink);
        }
        std::sort(evts.begin(), evts.end(),
                  [](const ParamEventSnap& a, const ParamEventSnap& b) { return a.beat < b.beat; });
        return evts;
    };

    bool anySolo = std::any_of(tl.tracks.begin(), tl.tracks.end(),
                               [](const Track& t) { return t.solo; });

    if (loopMode) {
        if (!aps) return;
        const auto& actives = aps->patterns();
        int trackIdx = 0;
        for (const Track& track : tl.tracks) {
            TrackSnap ts;
            int lanePatId = track.lanes.empty() ? 0 : track.lanes[0].patternId;
            if (!track.mute && (!anySolo || track.solo) && actives.count(lanePatId)) {
                float anchorBar = actives.at(lanePatId);
                const Pattern* pat = nullptr;
                for (const auto& p : tl.patterns)
                    if (p.id == lanePatId) { pat = &p; break; }
                if (pat && pat->lengthBeats > 0.0f) {
                    InstanceSnap is;
                    is.startBar     = anchorBar;
                    is.length       = 1.0e9f;
                    is.startOffset  = 0.0f;
                    is.patternBeats = pat->lengthBeats;
                    is.loop         = true;
                    int top, bot;
                    timeline->timeSigAt((int)std::max(0.0f, anchorBar), top, bot);
                    is.beatsPerBar = (float)top;
                    buildNotes(is, pat, trackIdx);

                    // Param lanes for this active pattern. Build BEFORE moving
                    // `is` below — moving leaves is.portName empty.
                    for (const auto& lane : pat->paramLanes) {
                        auto evts = buildParamEvents(lane);
                        if (evts.empty()) continue;
                        ParamInstSnap pis;
                        pis.startBar     = anchorBar;
                        pis.length       = 1.0e9f;
                        pis.startOffset  = 0.0f;
                        pis.beatsPerBar  = (float)top;
                        pis.patternBeats = pat->lengthBeats;
                        pis.loop         = true;
                        pis.portName     = is.portName;
                        pis.midiChannel  = is.midiChannel;
                        pis.priority     = trackIdx + 1;
                        pis.ccNumber     = ccForType(lane.type);
                        pis.events       = std::move(evts);
                        newSnap.paramInsts.push_back(std::move(pis));
                    }

                    if (!is.notes.empty())
                        ts.instances.push_back(std::move(is));
                }
            }
            newSnap.tracks.push_back(std::move(ts));
            ++trackIdx;
        }
    } else {
        int trackIdx = 0;
        for (const Track& track : tl.tracks) {
            TrackSnap ts;
            if (track.mute || (anySolo && !track.solo)) {
                newSnap.tracks.push_back(std::move(ts));
                ++trackIdx;
                continue;
            }
            if (track.lanes.empty()) { newSnap.tracks.push_back(std::move(ts)); ++trackIdx; continue; }
            for (const PatternInstance& inst : track.lanes[0].patterns) {
                const Pattern* pat = timeline->patternForInstance(inst.id);
                if (!pat || pat->lengthBeats <= 0.0f) continue;

                InstanceSnap is;
                is.startBar     = inst.startBar;
                is.length       = inst.length;
                is.startOffset  = inst.startOffset;
                is.patternBeats = pat->lengthBeats;
                int top, bot;
                timeline->timeSigAt((int)inst.startBar, top, bot);
                is.beatsPerBar = (float)top;
                buildNotes(is, pat, trackIdx);

                // Param lanes for this pattern instance. Build BEFORE moving `is`
                // into ts.instances below — moving leaves is.portName empty.
                if (!is.portName.empty()) {
                    for (const auto& lane : pat->paramLanes) {
                        auto evts = buildParamEvents(lane);
                        if (evts.empty()) continue;
                        ParamInstSnap pis;
                        pis.startBar     = inst.startBar;
                        pis.length       = inst.length;
                        pis.startOffset  = inst.startOffset;
                        pis.beatsPerBar  = (float)top;
                        pis.patternBeats = pat->lengthBeats;
                        pis.portName     = is.portName;
                        pis.midiChannel  = is.midiChannel;
                        pis.priority     = trackIdx + 1;
                        pis.ccNumber     = ccForType(lane.type);
                        pis.events       = std::move(evts);
                        newSnap.paramInsts.push_back(std::move(pis));
                    }
                }
                ts.instances.push_back(std::move(is));
            }
            newSnap.tracks.push_back(std::move(ts));
            ++trackIdx;
        }

        // Song-level param lanes: each routes only to its owning instrument's port,
        // priority 0 (lowest). Beat field stores bar position (1 unit = 1 bar).
        for (const auto& lane : tl.paramLanes) {
            if (lane.instrumentId == 0) continue;
            auto rit = instrumentMap_.find(lane.instrumentId);
            if (rit == instrumentMap_.end() || rit->second.portName.empty()) continue;
            auto evts = buildParamEvents(lane);
            if (evts.empty()) continue;
            ParamInstSnap pis;
            pis.startBar     = 0.0f;
            pis.length       = 1.0e9f;
            pis.startOffset  = 0.0f;
            pis.beatsPerBar  = 1.0f;  // 1 beat = 1 bar at song level
            pis.patternBeats = 0.0f;  // sentinel: song-level, no loop
            pis.portName     = rit->second.portName;
            pis.midiChannel  = rit->second.midiChannel - 1;  // store 0-based
            pis.priority     = 0;
            pis.ccNumber     = ccForType(lane.type);
            pis.events       = std::move(evts);
            newSnap.paramInsts.push_back(std::move(pis));
        }
    }

    {
        std::lock_guard<std::mutex> lock(snapMutex);
        snap = std::move(newSnap);
    }
}

// ── RT-safe bar/seconds conversion ────────────────────────────────────────────

double JackTransport::snapBarToSeconds(float bar) const
{
    for (int i = 0; i + 1 < (int)snap.segs.size(); i++) {
        if (bar < snap.segs[i + 1].bar) {
            double secsPerBar = snap.segs[i].beatsPerBar * 60.0 / snap.segs[i].bpm;
            return snap.segs[i].startSecs + (bar - snap.segs[i].bar) * secsPerBar;
        }
    }
    if (!snap.segs.empty()) {
        auto& last = snap.segs.back();
        double secsPerBar = last.beatsPerBar * 60.0 / last.bpm;
        return last.startSecs + (bar - last.bar) * secsPerBar;
    }
    return 0.0;
}

float JackTransport::snapSecondsToBar(double secs) const
{
    for (int i = 0; i + 1 < (int)snap.segs.size(); i++) {
        double segStart = snap.segs[i].startSecs;
        double segEnd   = snap.segs[i + 1].startSecs;
        if (secs < segEnd) {
            double secsPerBar = snap.segs[i].beatsPerBar * 60.0 / snap.segs[i].bpm;
            return snap.segs[i].bar + (float)((secs - segStart) / secsPerBar);
        }
    }
    if (!snap.segs.empty()) {
        auto& last = snap.segs.back();
        double secsPerBar = last.beatsPerBar * 60.0 / last.bpm;
        return last.bar + (float)((secs - last.startSecs) / secsPerBar);
    }
    return 0.0f;
}

// ── ITransport methods (UI thread) ───────────────────────────────────────────

void JackTransport::play()  { if (client && jackAlive.load()) jack_transport_start(client); }
void JackTransport::pause() { if (client && jackAlive.load()) jack_transport_stop(client); }
void JackTransport::rewind(){ if (client && jackAlive.load()) jack_transport_locate(client, 0); }

void JackTransport::seek(float bars)
{
    if (!client || !jackAlive.load() || !timeline) return;
    double secs  = timeline->barToSeconds(std::max(0.0f, bars));
    auto   frame = static_cast<jack_nframes_t>(secs * sampleRate);
    jack_transport_locate(client, frame);
}

float JackTransport::position() const
{
    if (!timeline) return 0.0f;
    double secs = static_cast<double>(posFrames.load()) / sampleRate;
    return static_cast<float>(timeline->secondsToBar(secs));
}

// ── JACK process callback (RT thread) ────────────────────────────────────────

int JackTransport::processCallback(jack_nframes_t nframes, void* arg)
{
    return static_cast<JackTransport*>(arg)->process(nframes);
}

// Find buffer for a named port; returns nullptr if not found.
static void* findBuf(const std::vector<std::pair<std::string, void*>>& bufs,
                     const std::string& name)
{
    for (const auto& [n, b] : bufs)
        if (n == name) return b;
    return nullptr;
}

int JackTransport::process(jack_nframes_t nframes)
{
    jack_position_t        pos;
    jack_transport_state_t state = jack_transport_query(client, &pos);
    bool nowPlaying = (state == JackTransportRolling);

    // Collect port buffers, keyed by name for per-channel routing.
    std::vector<NamedBuf> namedBufs;
    if (midiEnabled && portsMutex.try_lock()) {
        namedBufs.reserve(midiPorts_.size());
        for (auto& [name, port] : midiPorts_) {
            void* b = jack_port_get_buffer(port, nframes);
            jack_midi_clear_buffer(b);
            namedBufs.push_back({name, b});
        }
        portsMutex.unlock();
    }

    outEvents.clear();

    if (!namedBufs.empty() && pendingMutex_.try_lock()) {
        for (const auto& pm : pendingMsgs_) {
            void* b = findBuf(namedBufs, pm.portName);
            if (b) emit(b, 0, pm.data, pm.len);
        }
        pendingMsgs_.clear();
        pendingMutex_.unlock();
    }

    bool jumped = !firstCall && wasPlaying && (pos.frame != lastFrame + nframes);

    // On stop or jump: silence active notes, then reset all controllers on instrument channels.
    if (!namedBufs.empty() && ((!nowPlaying && wasPlaying) || jumped)) {
        for (auto& an : activeNotes) {
            uint8_t msg[3] = {
                static_cast<uint8_t>(0x80 | (an.channel & 0x0F)),
                static_cast<uint8_t>(an.midiPitch),
                0
            };
            void* b = findBuf(namedBufs, an.portName);
            if (b) emit(b, 0, msg, 3);
        }
        activeNotes.clear();

        if (snapMutex.try_lock()) {
            std::set<std::pair<std::string,int>> seen;
            for (const auto& track : snap.tracks)
                for (const auto& inst : track.instances)
                    if (!inst.portName.empty())
                        seen.insert({inst.portName, inst.midiChannel});
            for (const auto& [port, ch] : seen) {
                void* buf = findBuf(namedBufs, port);
                if (!buf) continue;
                uint8_t base = static_cast<uint8_t>(0xB0 | (ch & 0x0F));
                uint8_t resetMsg[3]  = { base, 121, 0 };  // Reset All Controllers
                uint8_t allOffMsg[3] = { base, 123, 0 };  // All Notes Off
                emit(buf, 0, resetMsg,  3);
                emit(buf, 0, allOffMsg, 3);
            }
            snapMutex.unlock();
        }
    }

    if (nowPlaying && !namedBufs.empty()) {
        jack_nframes_t blockEnd = pos.frame + nframes;

        // Fire note-offs for notes ending in this block.
        for (auto it = activeNotes.begin(); it != activeNotes.end(); ) {
            if (it->offFrame <= blockEnd) {
                jack_nframes_t off = (it->offFrame > pos.frame)
                    ? (it->offFrame - pos.frame) : 0;
                off = std::min(off, nframes - 1);
                uint8_t msg[3] = {
                    static_cast<uint8_t>(0x80 | (it->channel & 0x0F)),
                    static_cast<uint8_t>(it->midiPitch),
                    0
                };
                void* b = findBuf(namedBufs, it->portName);
                if (b) emit(b, off, msg, 3);
                it = activeNotes.erase(it);
            } else {
                ++it;
            }
        }

        if (snapMutex.try_lock()) {
            float prevBars = snapSecondsToBar(static_cast<double>(
                                 (firstCall || jumped) ? pos.frame : lastFrame) / sampleRate);
            float curBars  = snapSecondsToBar(static_cast<double>(blockEnd) / sampleRate);

            fireNoteEvents (namedBufs, nframes, pos.frame, prevBars, curBars);
            fireParamEvents(namedBufs, nframes, pos.frame, prevBars, curBars);
            snapMutex.unlock();
        }
    }

    // Flush the cycle's events in non-decreasing frame order per buffer. JACK
    // drops any event written out of frame order, so this single ordered pass
    // replaces the previously interleaved note-off / note-on / param writes.
    // seq is the insertion index, used as a total-order tiebreaker so this can
    // be an in-place std::sort (no temp-buffer allocation on the RT thread,
    // unlike std::stable_sort) while still keeping same-frame insertion order
    // (pending, note-offs, then note-ons / params) — so an off at a given frame
    // still precedes an on. n is the events in one ~ms cycle, so n log n is tiny.
    std::sort(outEvents.begin(), outEvents.end(),
        [](const OutEvent& a, const OutEvent& b) {
            if (a.buf   != b.buf)   return a.buf   < b.buf;
            if (a.frame != b.frame) return a.frame < b.frame;
            return a.seq < b.seq;
        });
    for (const OutEvent& e : outEvents)
        jack_midi_event_write(e.buf, e.frame, e.data, e.len);

    posFrames.store(pos.frame + nframes);
    playing_.store(nowPlaying);

    if ((nowPlaying != wasPlaying || jumped) && onTransportEvent)
        onTransportEvent();

    lastFrame  = pos.frame;
    wasPlaying = nowPlaying;
    firstCall  = false;

    return 0;
}

// ── Output event collection (RT thread) ──────────────────────────────────────

void JackTransport::emit(void* buf, jack_nframes_t frame,
                          const uint8_t* data, int len)
{
    OutEvent e{buf, frame, static_cast<uint32_t>(outEvents.size()), {}, len};
    for (int i = 0; i < len && i < 3; ++i)
        e.data[i] = data[i];
    outEvents.push_back(e);
}

// ── Note event generation (RT thread, called with snapMutex held) ─────────────

void JackTransport::fireNoteEvents(const std::vector<NamedBuf>& namedBufs,
                                    jack_nframes_t nframes,
                                    jack_nframes_t blockStart,
                                    float prevBars, float curBars)
{
    for (const TrackSnap& track : snap.tracks) {
        for (const InstanceSnap& inst : track.instances) {
            if (inst.patternBeats <= 0.0f)               continue;
            if (inst.notes.empty())                      continue;
            // Loop instances play forever; the anchor bar is only a phase
            // reference, so fire across the whole transport window (the wrap math
            // handles a playhead that is before the anchor). Finite (song) instances
            // are clamped to their placement.
            if (!inst.loop) {
                if (inst.startBar + inst.length <= prevBars) continue;
                if (inst.startBar >= curBars)                continue;
            }

            void* instBuf = findBuf(namedBufs, inst.portName);
            if (!instBuf) continue;

            float windowStart = inst.loop ? prevBars : std::max(prevBars, inst.startBar);
            float windowEnd   = inst.loop ? curBars  : std::min(curBars,  inst.startBar + inst.length);
            if (windowEnd <= windowStart) continue;
            float beatStart   = inst.startOffset + (windowStart - inst.startBar) * inst.beatsPerBar;
            float beatEnd     = inst.startOffset + (windowEnd   - inst.startBar) * inst.beatsPerBar;

            for (const NoteSnap& note : inst.notes) {
                float len = inst.patternBeats;

                float cycles    = std::floor((beatStart - note.beat) / len);
                float firstFire = note.beat + cycles * len;
                if (firstFire < beatStart) firstFire += len;

                while (firstFire < beatEnd) {
                    float onBar = inst.startBar
                                + (firstFire - inst.startOffset) / inst.beatsPerBar;
                    double onSecs = snapBarToSeconds(onBar);

                    auto onFrame = static_cast<jack_nframes_t>(onSecs * sampleRate);
                    jack_nframes_t onOff = (onFrame >= blockStart)
                        ? (onFrame - blockStart) : 0;
                    onOff = std::min(onOff, nframes - 1);

                    uint8_t vel = static_cast<uint8_t>(
                        std::clamp(static_cast<int>(note.velocity * 127), 1, 127));
                    uint8_t onMsg[3] = {
                        static_cast<uint8_t>(0x90 | (inst.midiChannel & 0x0F)),
                        static_cast<uint8_t>(note.midiPitch),
                        vel
                    };
                    emit(instBuf, onOff, onMsg, 3);

                    float offBar = inst.startBar
                                 + (firstFire + note.length - inst.startOffset) / inst.beatsPerBar;
                    auto offFrame = static_cast<jack_nframes_t>(
                        snapBarToSeconds(offBar) * sampleRate);

                    activeNotes.push_back({note.midiPitch, inst.midiChannel, offFrame,
                                           inst.portName});

                    firstFire += len;
                }
            }
        }
    }
}

// ── Param event generation (RT thread, called with snapMutex held) ───────────

void JackTransport::fireParamEvents(const std::vector<NamedBuf>& namedBufs,
                                     jack_nframes_t nframes,
                                     jack_nframes_t blockStart,
                                     float prevBars, float curBars)
{
    struct PendingParam {
        jack_nframes_t frame;
        const std::string* portName;
        int   midiChannel;
        int   ccNumber;
        int   value;
        int   priority;
    };
    std::vector<PendingParam> pending;

    for (const ParamInstSnap& inst : snap.paramInsts) {
        if (inst.events.empty()) continue;

        if (inst.patternBeats == 0.0f) {
            // Song-level: event.beat is a bar position; no looping.
            void* buf = findBuf(namedBufs, inst.portName);
            if (!buf) continue;
            for (const ParamEventSnap& evt : inst.events) {
                if (evt.beat < prevBars || evt.beat >= curBars) continue;
                double secs = snapBarToSeconds(evt.beat);
                auto   fr   = static_cast<jack_nframes_t>(secs * sampleRate);
                jack_nframes_t off = (fr >= blockStart) ? (fr - blockStart) : 0;
                off = std::min(off, nframes - 1);
                pending.push_back({off, &inst.portName, inst.midiChannel,
                                   inst.ccNumber, evt.value, inst.priority});
            }
        } else {
            // Pattern-level: event.beat is a within-pattern beat; wraps with patternBeats.
            // Loop instances play forever (anchor is phase only); finite (song)
            // instances are clamped to their placement.
            if (!inst.loop) {
                if (inst.startBar + inst.length <= prevBars) continue;
                if (inst.startBar >= curBars)                continue;
            }

            void* buf = findBuf(namedBufs, inst.portName);
            if (!buf) continue;

            float windowStart = inst.loop ? prevBars : std::max(prevBars, inst.startBar);
            float windowEnd   = inst.loop ? curBars  : std::min(curBars,  inst.startBar + inst.length);
            if (windowEnd <= windowStart) continue;
            float beatStart   = inst.startOffset + (windowStart - inst.startBar) * inst.beatsPerBar;
            float beatEnd     = inst.startOffset + (windowEnd   - inst.startBar) * inst.beatsPerBar;

            for (const ParamEventSnap& evt : inst.events) {
                float len       = inst.patternBeats;
                float cycles    = std::floor((beatStart - evt.beat) / len);
                float firstFire = evt.beat + cycles * len;
                if (firstFire < beatStart) firstFire += len;

                while (firstFire < beatEnd) {
                    float  onBar  = inst.startBar + (firstFire - inst.startOffset) / inst.beatsPerBar;
                    double onSecs = snapBarToSeconds(onBar);
                    auto   fr     = static_cast<jack_nframes_t>(onSecs * sampleRate);
                    jack_nframes_t off = (fr >= blockStart) ? (fr - blockStart) : 0;
                    off = std::min(off, nframes - 1);
                    pending.push_back({off, &inst.portName, inst.midiChannel,
                                       inst.ccNumber, evt.value, inst.priority});
                    firstFire += len;
                }
            }
        }
    }

    if (pending.empty()) return;

    // Sort by (portName, channel, cc, frame) then priority descending so the
    // highest-priority entry for each combination comes first.
    std::sort(pending.begin(), pending.end(), [](const PendingParam& a, const PendingParam& b) {
        if (a.portName    != b.portName)    return *a.portName    < *b.portName;
        if (a.midiChannel != b.midiChannel) return a.midiChannel  < b.midiChannel;
        if (a.ccNumber    != b.ccNumber)    return a.ccNumber      < b.ccNumber;
        if (a.frame       != b.frame)       return a.frame         < b.frame;
        return a.priority > b.priority;   // higher priority first within same slot
    });

    for (int i = 0; i < (int)pending.size(); ) {
        const PendingParam& p = pending[i];
        // Advance past lower-priority duplicates for the same (port,ch,cc,frame).
        int j = i + 1;
        while (j < (int)pending.size()       &&
               *pending[j].portName == *p.portName &&
               pending[j].midiChannel == p.midiChannel &&
               pending[j].ccNumber    == p.ccNumber    &&
               pending[j].frame       == p.frame)
            ++j;

        void* buf = findBuf(namedBufs, *p.portName);
        if (buf) {
            if (p.ccNumber < 0) {
                // Pitch bend: value is already 0-16383 (14-bit).
                int     val14 = p.value;
                uint8_t msg[3] = {
                    static_cast<uint8_t>(0xE0 | (p.midiChannel & 0x0F)),
                    static_cast<uint8_t>(val14 & 0x7F),
                    static_cast<uint8_t>((val14 >> 7) & 0x7F)
                };
                emit(buf, p.frame, msg, 3);
            } else {
                uint8_t msg[3] = {
                    static_cast<uint8_t>(0xB0 | (p.midiChannel & 0x0F)),
                    static_cast<uint8_t>(p.ccNumber & 0x7F),
                    static_cast<uint8_t>(p.value    & 0x7F)
                };
                emit(buf, p.frame, msg, 3);
            }
        }
        i = j;
    }
}

// ── Port management (UI thread) ───────────────────────────────────────────────

bool JackTransport::addMidiPort(const std::string& name) {
    if (!client || !midiEnabled || name.empty()) return false;
    jack_port_t* p = jack_port_register(client, name.c_str(),
                                         JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
    if (!p) {
        fprintf(stderr, "JackTransport: could not register port '%s'\n", name.c_str());
        return false;
    }
    std::lock_guard<std::mutex> lk(portsMutex);
    midiPorts_[name] = p;
    return true;
}

bool JackTransport::removeMidiPort(const std::string& name) {
    if (!client) return false;
    jack_port_t* p = nullptr;
    {
        std::lock_guard<std::mutex> lk(portsMutex);
        auto it = midiPorts_.find(name);
        if (it == midiPorts_.end()) return false;
        p = it->second;
        midiPorts_.erase(it);
    }
    jack_port_unregister(client, p);
    return true;
}

bool JackTransport::renameMidiPort(const std::string& oldName, const std::string& newName) {
    if (!client || newName.empty()) return false;
    std::lock_guard<std::mutex> lk(portsMutex);
    auto it = midiPorts_.find(oldName);
    if (it == midiPorts_.end()) return false;
    jack_port_t* p = it->second;
    if (jack_port_rename(client, p, newName.c_str()) != 0) {
        fprintf(stderr, "JackTransport: could not rename port '%s' -> '%s'\n",
                oldName.c_str(), newName.c_str());
        return false;
    }
    midiPorts_.erase(it);
    midiPorts_[newName] = p;
    return true;
}
