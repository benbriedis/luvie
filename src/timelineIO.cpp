#include "timelineIO.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

// ── Note ─────────────────────────────────────────────────────────────────────

static json noteToJson(const Note& n) {
    return {
        {"id",             n.id},
        {"row",            n.row},
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
    n.row            = j.at("row");
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
    json jdrumSolo = json::array();
    for (int n : p.drumSolo) jdrumSolo.push_back(n);
    json jdrumMute = json::array();
    for (int n : p.drumMute) jdrumMute.push_back(n);
    return {
        {"id",           p.id},
        {"lengthBeats",  p.lengthBeats},
        {"type",         (int)p.type},
        {"notes",        jnotes},
        {"drumNotes",    jdrum},
        {"drumSolo",     jdrumSolo},
        {"drumMute",     jdrumMute},
        {"instrumentId", p.instrumentId},
        {"name",         p.name},
        {"timeSigTop",   p.timeSigTop},
        {"timeSigBottom",p.timeSigBottom},
    };
}

static Pattern patternFromJson(const json& j) {
    Pattern p;
    p.id           = j.at("id");
    p.lengthBeats  = j.at("lengthBeats");
    p.type         = (PatternType)j.value("type", 0);
    p.instrumentId = j.value("instrumentId", 0);
    p.name         = j.value("name", "");
    p.timeSigTop   = j.value("timeSigTop",    4);
    p.timeSigBottom= j.value("timeSigBottom", 4);
    for (const auto& jn : j.value("notes",     json::array())) p.notes.push_back(noteFromJson(jn));
    for (const auto& jd : j.value("drumNotes", json::array())) p.drumNotes.push_back(drumNoteFromJson(jd));
    for (int n : j.value("drumSolo", json::array())) p.drumSolo.insert(n);
    for (int n : j.value("drumMute", json::array())) p.drumMute.insert(n);
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

static json laneToJson(const Lane& l) {
    json jinsts = json::array();
    for (const auto& inst : l.patterns) jinsts.push_back(instanceToJson(inst));
    return {{"id", l.id}, {"patternId", l.patternId}, {"patterns", jinsts}};
}

static json trackToJson(const Track& t) {
    json jlanes = json::array();
    for (const auto& l : t.lanes) jlanes.push_back(laneToJson(l));
    json jinsts = json::array();
    if (!t.lanes.empty())
        for (const auto& inst : t.lanes[0].patterns) jinsts.push_back(instanceToJson(inst));
    return {
        {"id",           t.id},
        {"instrumentId", t.instrumentId},
        {"patternId",    t.lanes.empty() ? 0 : t.lanes[0].patternId},
        {"solo",         t.solo},
        {"mute",         t.mute},
        {"stackedLanes", t.stackedLanes},
        {"patterns",     jinsts},
        {"lanes",        jlanes},
    };
}

static Track trackFromJson(const json& j) {
    Track t;
    t.id           = j.at("id");
    t.instrumentId = j.value("instrumentId", 0);
    t.solo         = j.value("solo", false);
    t.mute         = j.value("mute", false);
    t.stackedLanes = j.value("stackedLanes", false);

    if (j.contains("lanes") && j.at("lanes").is_array() && !j.at("lanes").empty()) {
        for (const auto& jl : j.at("lanes")) {
            Lane lane;
            lane.id        = jl.value("id", 0);
            lane.patternId = jl.value("patternId", 0);
            for (const auto& jinst : jl.value("patterns", json::array()))
                lane.patterns.push_back(instanceFromJson(jinst));
            t.lanes.push_back(std::move(lane));
        }
    } else {
        Lane lane;
        lane.id        = 0;
        lane.patternId = j.value("patternId", 0);
        for (const auto& jinst : j.value("patterns", json::array()))
            lane.patterns.push_back(instanceFromJson(jinst));
        t.lanes.push_back(std::move(lane));
    }
    return t;
}

// ── ParamLane ─────────────────────────────────────────────────────────────────

static json paramPointToJson(const ParamPoint& p) {
    return {{"id", p.id}, {"beat", p.beat}, {"value", p.value}, {"anchor", p.anchor}};
}
static ParamPoint paramPointFromJson(const json& j) {
    return {j.at("id"), j.at("beat"), j.value("value", 63), j.value("anchor", false)};
}
static json paramLaneToJson(const ParamLane& lane) {
    json jpts = json::array();
    for (const auto& p : lane.points) jpts.push_back(paramPointToJson(p));
    return {{"id", lane.id}, {"type", lane.type}, {"points", jpts}};
}
static ParamLane paramLaneFromJson(const json& j) {
    ParamLane lane;
    lane.id   = j.at("id");
    lane.type = j.at("type").get<std::string>();
    for (const auto& jp : j.value("points", json::array()))
        lane.points.push_back(paramPointFromJson(jp));
    return lane;
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

    json jinstrs = json::array();
    for (const auto& i : tl.instruments)
        jinstrs.push_back({{"id", i.id}, {"name", i.name}, {"isDrum", i.isDrum}});

    json jparams = json::array();
    for (const auto& lane : tl.paramLanes) jparams.push_back(paramLaneToJson(lane));

    json jrows = json::array();
    for (const auto& r : tl.rowOrder)
        if (r.kind != RowKind::Header)
            jrows.push_back({{"isTrack", r.kind == RowKind::Lane}, {"id", r.id}});

    return {
        {"bpms",               jbpms},
        {"timeSigs",           jsigs},
        {"patterns",           jpats},
        {"tracks",             jtracks},
        {"instruments",        jinstrs},
        {"paramLanes",         jparams},
        {"rowOrder",           jrows},
        {"selectedTrackIndex", tl.selectedTrackIndex},
        {"selectedLaneId",     tl.selectedLaneId},
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
    for (const auto& ji : j.value("instruments", json::array()))
        tl.instruments.push_back({ji.at("id"), ji.at("name").get<std::string>(), ji.value("isDrum", false)});
    for (const auto& jp : j.value("paramLanes", json::array()))
        tl.paramLanes.push_back(paramLaneFromJson(jp));
    for (const auto& jr : j.value("rowOrder", json::array()))
        tl.rowOrder.push_back({jr.at("isTrack").get<bool>() ? RowKind::Lane : RowKind::Param, jr.at("id").get<int>()});
    tl.selectedTrackIndex = j.value("selectedTrackIndex", -1);
    tl.selectedLaneId     = j.value("selectedLaneId", -1);
    return tl;
}

// ── Public API ────────────────────────────────────────────────────────────────

std::string appStateToJsonString(const AppState& state) {
    json jconns = json::array();
    for (const auto& c : state.jackOutputs)
        jconns.push_back({{"portName", c.portName}});
    json jinstrs = json::array();
    for (const auto& c : state.jackInstruments) {
        json jmap;
        for (const auto& [note, name] : c.drumMap)
            jmap[std::to_string(note)] = name;
        jinstrs.push_back({{"id", c.id}, {"name", c.name}, {"portName", c.portName},
                           {"midiChannel", c.midiChannel}, {"drumMap", jmap},
                           {"isDrum", c.isDrum}, {"fallbackNoteNames", c.fallbackNoteNames},
                           {"programNumber", c.programNumber},
                           {"bankMsb", c.bankMsb}, {"bankLsb", c.bankLsb},
                           {"gm1Instrument", c.gm1Instrument}});
    }
    json j = {
        {"version",         1},
        {"rootPitch",       state.rootPitch},
        {"chordType",       state.chordType},
        {"sharp",           state.sharp},
        {"transport",       state.transport},
        {"timeline",        timelineToJson(state.timeline)},
        {"jackOutputs",     jconns},
        {"jackInstruments", jinstrs},
    };
    return j.dump(2);
}

bool appStateFromJsonString(const std::string& jsonStr, AppState& state) {
    json j;
    try {
        j = json::parse(jsonStr);
    } catch (...) {
        return false;
    }
    state.rootPitch = j.value("rootPitch", 0);
    state.chordType = j.value("chordType", 0);
    state.sharp     = j.value("sharp",     true);
    state.transport = j.value("transport", -1);
    if (j.contains("timeline"))
        state.timeline = timelineFromJson(j.at("timeline"));
    for (const auto& jc : j.value("jackOutputs", json::array()))
        state.jackOutputs.push_back({jc.value("portName", "")});
    auto instrArray = j.contains("jackInstruments") ? j.value("jackInstruments", json::array())
                                                    : j.value("jackChannels",    json::array());
    for (const auto& jc : instrArray) {
        JackInstrument ch;
        ch.id          = jc.value("id", 0);
        ch.name        = jc.value("name", "");
        ch.portName    = jc.value("portName", "");
        ch.midiChannel = jc.value("midiChannel", 1);
        if (jc.contains("drumMap"))
            for (auto& [k, v] : jc["drumMap"].items())
                ch.drumMap[std::stoi(k)] = v.get<std::string>();
        ch.isDrum            = jc.value("isDrum",            false);
        ch.fallbackNoteNames = jc.value("fallbackNoteNames", false);
        ch.programNumber     = jc.value("programNumber",     -1);
        ch.bankMsb           = jc.value("bankMsb",           -1);
        ch.bankLsb           = jc.value("bankLsb",           -1);
        ch.gm1Instrument     = jc.value("gm1Instrument",     -1);
        state.jackInstruments.push_back(std::move(ch));
    }
    return true;
}

bool saveAppState(const AppState& state, const std::string& filePath) {
    std::ofstream f(filePath);
    if (!f) return false;
    f << appStateToJsonString(state);
    return f.good();
}

bool loadAppState(const std::string& filePath, AppState& state) {
    std::ifstream f(filePath);
    if (!f) return false;
    std::string str((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return appStateFromJsonString(str, state);
}
