#include "timelineIO.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

// ── Note ─────────────────────────────────────────────────────────────────────

static json noteToJson(const Note& n) {
    return {
        {"id",             n.id},
        {"pitch",          n.pitch},
        {"beat",           n.beat},
        {"length",         n.length},
        {"velocity",       n.velocity},
        {"disabled",       n.disabled},
        {"disabledDegree", n.disabledDegree},
    };
}

static Note noteFromJson(const json& j) {
    Note n;
    n.id             = j.at("id");
    n.pitch          = j.at("pitch");
    n.beat           = j.at("beat");
    n.length         = j.at("length");
    n.velocity       = j.value("velocity", 0.0f);
    n.disabled       = j.value("disabled", false);
    n.disabledDegree = j.value("disabledDegree", -1);
    return n;
}

// ── DrumNote ─────────────────────────────────────────────────────────────────

static json drumNoteToJson(const DrumNote& d) {
    return {
        {"id",       d.id},
        {"note",     d.note},
        {"beat",     d.beat},
        {"velocity", d.velocity},
    };
}

static DrumNote drumNoteFromJson(const json& j) {
    DrumNote d;
    d.id       = j.at("id");
    d.note     = j.at("note");
    d.beat     = j.at("beat");
    d.velocity = j.value("velocity", 0.8f);
    return d;
}

// ── Pattern ───────────────────────────────────────────────────────────────────

static json patternToJson(const Pattern& p) {
    json jnotes = json::array();
    for (const auto& n : p.notes)     jnotes.push_back(noteToJson(n));
    json jdrum = json::array();
    for (const auto& d : p.drumNotes) jdrum.push_back(drumNoteToJson(d));
    return {
        {"id",           p.id},
        {"lengthBeats",  p.lengthBeats},
        {"type",         (int)p.type},
        {"notes",        jnotes},
        {"drumNotes",    jdrum},
    };
}

static Pattern patternFromJson(const json& j) {
    Pattern p;
    p.id          = j.at("id");
    p.lengthBeats = j.at("lengthBeats");
    p.type        = (PatternType)j.value("type", 0);
    for (const auto& jn : j.value("notes",     json::array())) p.notes.push_back(noteFromJson(jn));
    for (const auto& jd : j.value("drumNotes", json::array())) p.drumNotes.push_back(drumNoteFromJson(jd));
    return p;
}

// ── PatternInstance ───────────────────────────────────────────────────────────

static json instanceToJson(const PatternInstance& inst) {
    return {
        {"id",          inst.id},
        {"patternId",   inst.patternId},
        {"startBar",    inst.startBar},
        {"length",      inst.length},
        {"startOffset", inst.startOffset},
    };
}

static PatternInstance instanceFromJson(const json& j) {
    PatternInstance inst;
    inst.id          = j.at("id");
    inst.patternId   = j.at("patternId");
    inst.startBar    = j.at("startBar");
    inst.length      = j.at("length");
    inst.startOffset = j.value("startOffset", 0.0f);
    return inst;
}

// ── Track ─────────────────────────────────────────────────────────────────────

static json trackToJson(const Track& t) {
    json jinsts = json::array();
    for (const auto& inst : t.patterns) jinsts.push_back(instanceToJson(inst));
    return {
        {"id",        t.id},
        {"label",     t.label},
        {"patternId", t.patternId},
        {"patterns",  jinsts},
    };
}

static Track trackFromJson(const json& j) {
    Track t;
    t.id        = j.at("id");
    t.label     = j.at("label");
    t.patternId = j.value("patternId", 0);
    for (const auto& jinst : j.value("patterns", json::array())) t.patterns.push_back(instanceFromJson(jinst));
    return t;
}

// ── Timeline ──────────────────────────────────────────────────────────────────

static json timelineToJson(const Timeline& tl) {
    json jbpms = json::array();
    for (const auto& b : tl.bpms) jbpms.push_back({{"bar", b.bar}, {"bpm", b.bpm}});

    json jsigs = json::array();
    for (const auto& s : tl.timeSigs) jsigs.push_back({{"bar", s.bar}, {"top", s.top}, {"bottom", s.bottom}});

    json jpats = json::array();
    for (const auto& p : tl.patterns) jpats.push_back(patternToJson(p));

    json jtracks = json::array();
    for (const auto& t : tl.tracks) jtracks.push_back(trackToJson(t));

    return {
        {"bpms",               jbpms},
        {"timeSigs",           jsigs},
        {"patterns",           jpats},
        {"tracks",             jtracks},
        {"selectedTrackIndex", tl.selectedTrackIndex},
    };
}

static Timeline timelineFromJson(const json& j) {
    Timeline tl;
    for (const auto& jb : j.value("bpms", json::array()))
        tl.bpms.push_back({jb.at("bar"), (float)jb.at("bpm")});
    for (const auto& js : j.value("timeSigs", json::array()))
        tl.timeSigs.push_back({js.at("bar"), (int)js.at("top"), (int)js.at("bottom")});
    for (const auto& jp : j.value("patterns", json::array()))
        tl.patterns.push_back(patternFromJson(jp));
    for (const auto& jt : j.value("tracks", json::array()))
        tl.tracks.push_back(trackFromJson(jt));
    tl.selectedTrackIndex = j.value("selectedTrackIndex", -1);
    return tl;
}

// ── Public API ────────────────────────────────────────────────────────────────

bool saveAppState(const AppState& state, const std::string& filePath) {
    json j = {
        {"version",   1},
        {"rootPitch", state.rootPitch},
        {"chordType", state.chordType},
        {"sharp",     state.sharp},
        {"timeline",  timelineToJson(state.timeline)},
    };
    std::ofstream f(filePath);
    if (!f) return false;
    f << j.dump(2);
    return f.good();
}

bool loadAppState(const std::string& filePath, AppState& state) {
    std::ifstream f(filePath);
    if (!f) return false;
    json j;
    try {
        f >> j;
    } catch (...) {
        return false;
    }
    state.rootPitch = j.value("rootPitch", 0);
    state.chordType = j.value("chordType", 0);
    state.sharp     = j.value("sharp",     true);
    if (j.contains("timeline"))
        state.timeline = timelineFromJson(j.at("timeline"));
    return true;
}
