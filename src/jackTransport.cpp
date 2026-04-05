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
    if (client) {
        jack_deactivate(client);
        jack_client_close(client);
    }
}

// ── Open ──────────────────────────────────────────────────────────────────────

bool JackTransport::open(const char* clientName)
{
    jack_status_t status;
    client = jack_client_open(clientName, JackNullOption, &status);
    if (!client) {
        fprintf(stderr, "JackTransport: could not connect to JACK (status 0x%x)\n", status);
        return false;
    }

    sampleRate = jack_get_sample_rate(client);

    midiOut = jack_port_register(client, "midi_out",
                                  JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
    if (!midiOut) {
        fprintf(stderr, "JackTransport: could not register MIDI output port\n");
        jack_client_close(client);
        client = nullptr;
        return false;
    }

    jack_set_process_callback(client, processCallback, this);

    if (jack_activate(client) != 0) {
        fprintf(stderr, "JackTransport: could not activate JACK client\n");
        jack_client_close(client);
        client = nullptr;
        return false;
    }

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

// ── Snapshot building (UI thread) ─────────────────────────────────────────────

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
        ts.channel = trackIdx % 16;

        for (const PatternInstance& inst : track.patterns) {
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

            for (const Note& note : pat->notes) {
                if (note.disabled) continue;
                int n = note.pitch;
                int midi = rootMidi0
                         + chordDefs[chordType].intervals[n % chordSize]
                         + (n / chordSize) * 12;
                midi = std::clamp(midi, 0, 127);
                is.notes.push_back({midi, note.beat, note.length, note.velocity});
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

// ── RT-safe bar/seconds conversion (call with snapMutex held) ─────────────────

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

void JackTransport::play()
{
    if (client) jack_transport_start(client);
}

void JackTransport::pause()
{
    if (client) jack_transport_stop(client);
}

void JackTransport::rewind()
{
    if (client) jack_transport_locate(client, 0);
}

void JackTransport::seek(float bars)
{
    if (!client || !timeline) return;
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

int JackTransport::process(jack_nframes_t nframes)
{
    jack_position_t        pos;
    jack_transport_state_t state = jack_transport_query(client, &pos);
    bool nowPlaying = (state == JackTransportRolling);

    void* portBuf = jack_port_get_buffer(midiOut, nframes);
    jack_midi_clear_buffer(portBuf);

    // Detect a seek/jump (frame didn't advance by the expected amount).
    bool jumped = !firstCall && wasPlaying
                  && (pos.frame != lastFrame + nframes);

    // On stop or jump: silence all active notes immediately.
    if ((!nowPlaying && wasPlaying) || jumped) {
        for (auto& an : activeNotes) {
            uint8_t msg[3] = {
                static_cast<uint8_t>(0x80 | (an.channel & 0x0F)),
                static_cast<uint8_t>(an.midiPitch),
                0
            };
            jack_midi_event_write(portBuf, 0, msg, 3);
        }
        activeNotes.clear();
    }

    if (nowPlaying) {
        jack_nframes_t blockEnd = pos.frame + nframes;

        // Fire note-offs for notes ending within this block.
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
                jack_midi_event_write(portBuf, off, msg, 3);
                it = activeNotes.erase(it);
            } else {
                ++it;
            }
        }

        // Generate note-ons from timeline snapshot (non-blocking lock).
        if (snapMutex.try_lock()) {
            float prevBars = snapSecondsToBar(static_cast<double>(
                                 (firstCall || jumped) ? pos.frame : lastFrame) / sampleRate);
            float curBars  = snapSecondsToBar(static_cast<double>(blockEnd) / sampleRate);

            fireNoteEvents(portBuf, nframes, pos.frame, prevBars, curBars);
            snapMutex.unlock();
        }
    }

    posFrames.store(pos.frame + nframes);
    playing_.store(nowPlaying);
    lastFrame  = pos.frame;
    wasPlaying = nowPlaying;
    firstCall  = false;

    return 0;
}

// ── Note event generation (RT thread, called with snapMutex held) ─────────────

void JackTransport::fireNoteEvents(void* portBuf, jack_nframes_t nframes,
                                    jack_nframes_t blockStart,
                                    float prevBars, float curBars)
{
    for (const TrackSnap& track : snap.tracks) {
        for (const InstanceSnap& inst : track.instances) {
            if (inst.startBar + inst.length <= prevBars) continue;
            if (inst.startBar >= curBars)                continue;
            if (inst.patternBeats <= 0.0f)               continue;
            if (inst.notes.empty())                      continue;

            float windowStart = std::max(prevBars, inst.startBar);
            float windowEnd   = std::min(curBars,  inst.startBar + inst.length);
            float beatStart   = inst.startOffset + (windowStart - inst.startBar) * inst.beatsPerBar;
            float beatEnd     = inst.startOffset + (windowEnd   - inst.startBar) * inst.beatsPerBar;

            for (const NoteSnap& note : inst.notes) {
                float len = inst.patternBeats;

                // First firing at or after beatStart (handles pattern looping).
                float cycles    = std::floor((beatStart - note.beat) / len);
                float firstFire = note.beat + cycles * len;
                if (firstFire < beatStart) firstFire += len;

                while (firstFire < beatEnd) {
                    // Song-bar position of this note-on.
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
                        static_cast<uint8_t>(0x90 | (track.channel & 0x0F)),
                        static_cast<uint8_t>(note.midiPitch),
                        vel
                    };
                    jack_midi_event_write(portBuf, onOff, onMsg, 3);

                    // Schedule note-off.
                    float offBar = inst.startBar
                                 + (firstFire + note.length - inst.startOffset) / inst.beatsPerBar;
                    auto offFrame = static_cast<jack_nframes_t>(
                        snapBarToSeconds(offBar) * sampleRate);

                    activeNotes.push_back({note.midiPitch, track.channel, offFrame});

                    firstFire += len;  // advance to next pattern loop
                }
            }
        }
    }
}
