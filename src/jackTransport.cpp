#include "jackTransport.hpp"
#include "chords.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

// ── Construction / destruction ────────────────────────────────────────────────

JackTransport::JackTransport() {}

JackTransport::~JackTransport()
{
    if (timeline) timeline->removeObserver(this);
    if (client && jackAlive.load()) {
        jack_deactivate(client);
        for (auto& [name, port] : midiPorts_)
            jack_port_unregister(client, port);
        jack_client_close(client);
    }
}

// ── Open ──────────────────────────────────────────────────────────────────────

bool JackTransport::open(const char* clientName, bool enableMidi)
{
    midiEnabled = enableMidi;

    jack_status_t status;
    client = jack_client_open(clientName, JackNullOption, &status);
    if (!client) {
        fprintf(stderr, "JackTransport: could not connect to JACK (status 0x%x)\n", status);
        return false;
    }

    sampleRate = jack_get_sample_rate(client);

    jack_set_process_callback(client, processCallback, this);

    if (jack_activate(client) != 0) {
        fprintf(stderr, "JackTransport: could not activate JACK client\n");
        jack_client_close(client);
        client = nullptr;
        return false;
    }

    jackAlive.store(true);
    return true;
}

// ── UI-thread setters ─────────────────────────────────────────────────────────

void JackTransport::setTimeline(ObservableTimeline* tl)
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

void JackTransport::setChannels(const std::vector<ChannelRouting>& routings)
{
    channelMap_.clear();
    for (const auto& r : routings)
        channelMap_[r.channelName] = r;
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
    int chordSize = chordDefs[chordType].size;
    int rootSemi  = (rootPitch + 9) % 12;
    int rootMidi0 = 12 + rootSemi;

    int trackIdx = 0;
    for (const Track& track : tl.tracks) {
        TrackSnap ts;

        for (const PatternInstance& inst : track.patterns) {
            const Pattern* pat = timeline->patternForInstance(inst.id);
            if (!pat || pat->lengthBeats <= 0.0f) continue;

            InstanceSnap is;
            is.startBar    = inst.startBar;
            is.length      = inst.length;
            is.startOffset = inst.startOffset;
            is.patternBeats = pat->lengthBeats;

            int top, bot;
            timeline->timeSigAt((int)inst.startBar, top, bot);
            is.beatsPerBar = (float)top;

            // Resolve output channel routing from the pattern's assignment.
            is.portName   = "";
            is.midiChannel = trackIdx % 16;  // fallback when no channel assigned
            if (!pat->outputChannelName.empty()) {
                auto it = channelMap_.find(pat->outputChannelName);
                if (it != channelMap_.end()) {
                    is.portName    = it->second.portName;
                    is.midiChannel = it->second.midiChannel - 1;  // to 0-based
                }
            }

            if (pat->type == PatternType::DRUM) {
                for (const DrumNote& dn : pat->drumNotes) {
                    int midi = std::clamp(dn.note, 0, 127);
                    is.notes.push_back({midi, dn.beat, drumNoteLen, dn.velocity});
                }
            } else {
                for (const Note& note : pat->notes) {
                    if (note.disabled) continue;
                    int n = note.pitch;
                    int midi = rootMidi0
                             + chordDefs[chordType].intervals[n % chordSize]
                             + (n / chordSize) * 12;
                    midi = std::clamp(midi, 0, 127);
                    is.notes.push_back({midi, note.beat, note.length, note.velocity});
                }
            }

            ts.instances.push_back(std::move(is));
        }

        newSnap.tracks.push_back(std::move(ts));
        ++trackIdx;
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
    std::vector<void*>    allBufs;
    if (midiEnabled && portsMutex.try_lock()) {
        namedBufs.reserve(midiPorts_.size());
        allBufs.reserve(midiPorts_.size());
        for (auto& [name, port] : midiPorts_) {
            void* b = jack_port_get_buffer(port, nframes);
            jack_midi_clear_buffer(b);
            namedBufs.push_back({name, b});
            allBufs.push_back(b);
        }
        portsMutex.unlock();
    }

    bool jumped = !firstCall && wasPlaying && (pos.frame != lastFrame + nframes);

    // On stop or jump: silence all active notes.
    if (!allBufs.empty() && ((!nowPlaying && wasPlaying) || jumped)) {
        for (auto& an : activeNotes) {
            uint8_t msg[3] = {
                static_cast<uint8_t>(0x80 | (an.channel & 0x0F)),
                static_cast<uint8_t>(an.midiPitch),
                0
            };
            // Send note-off to the specific port (or all if unrouted).
            if (an.portName.empty()) {
                for (void* b : allBufs) jack_midi_event_write(b, 0, msg, 3);
            } else {
                void* b = findBuf(namedBufs, an.portName);
                if (b) jack_midi_event_write(b, 0, msg, 3);
                else for (void* fb : allBufs) jack_midi_event_write(fb, 0, msg, 3);
            }
        }
        activeNotes.clear();
    }

    if (nowPlaying && !allBufs.empty()) {
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
                if (it->portName.empty()) {
                    for (void* b : allBufs) jack_midi_event_write(b, off, msg, 3);
                } else {
                    void* b = findBuf(namedBufs, it->portName);
                    if (b) jack_midi_event_write(b, off, msg, 3);
                    else for (void* fb : allBufs) jack_midi_event_write(fb, off, msg, 3);
                }
                it = activeNotes.erase(it);
            } else {
                ++it;
            }
        }

        if (snapMutex.try_lock()) {
            float prevBars = snapSecondsToBar(static_cast<double>(
                                 (firstCall || jumped) ? pos.frame : lastFrame) / sampleRate);
            float curBars  = snapSecondsToBar(static_cast<double>(blockEnd) / sampleRate);

            fireNoteEvents(namedBufs, allBufs, nframes, pos.frame, prevBars, curBars);
            snapMutex.unlock();
        }
    }

    posFrames.store(pos.frame + nframes);
    playing_.store(nowPlaying);

    if ((nowPlaying != wasPlaying || jumped) && onTransportEvent)
        onTransportEvent();

    lastFrame  = pos.frame;
    wasPlaying = nowPlaying;
    firstCall  = false;

    return 0;
}

// ── Note event generation (RT thread, called with snapMutex held) ─────────────

void JackTransport::fireNoteEvents(const std::vector<NamedBuf>& namedBufs,
                                    const std::vector<void*>& allBufs,
                                    jack_nframes_t nframes,
                                    jack_nframes_t blockStart,
                                    float prevBars, float curBars)
{
    for (const TrackSnap& track : snap.tracks) {
        for (const InstanceSnap& inst : track.instances) {
            if (inst.startBar + inst.length <= prevBars) continue;
            if (inst.startBar >= curBars)                continue;
            if (inst.patternBeats <= 0.0f)               continue;
            if (inst.notes.empty())                      continue;

            // Resolve output buffer for this instance.
            void* instBuf = nullptr;
            if (!inst.portName.empty())
                instBuf = findBuf(namedBufs, inst.portName);

            float windowStart = std::max(prevBars, inst.startBar);
            float windowEnd   = std::min(curBars,  inst.startBar + inst.length);
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
                    if (instBuf) {
                        jack_midi_event_write(instBuf, onOff, onMsg, 3);
                    } else {
                        for (void* b : allBufs) jack_midi_event_write(b, onOff, onMsg, 3);
                    }

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
