#include "luvie_dsp.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* -----------------------------------------------------------------------
   Hardcoded test timeline: one track, one pattern, a short arpeggio.
   Pitches are absolute MIDI note numbers.
   ----------------------------------------------------------------------- */

static DspNote testNotes[] = {
    /* bar, beat, length, pitch (MIDI), velocity, channel */
    { 0, 0.0f, 0.5f, 60, 100, 0 },   /* C4 */
    { 0, 1.0f, 0.5f, 64, 100, 0 },   /* E4 */
    { 0, 2.0f, 0.5f, 67, 100, 0 },   /* G4 */
    { 0, 3.0f, 0.5f, 72, 100, 0 },   /* C5 */
};

static DspPattern testPattern = {
    .id         = 1,
    .lengthBeats = 4.0f,
    .numNotes   = 4,
    .notes      = testNotes
};

static DspPatternInstance testInstance = {
    .id          = 1,
    .patternId   = 1,
    .startBar    = 0,
    .length      = 999.0f,   /* effectively unlimited */
    .startOffset = 0.0f
};

static DspTrack testTrack = {
    .id           = 1,
    .channel      = 0,
    .numInstances = 1,
    .instances    = &testInstance
};

static DspTimeSig testTimeSig = { .bar = 0, .beatsPerBar = 4, .beatUnit = 4 };
static DspBpmMarker testBpm   = { .bar = 0, .bpm = 120.0f };

/* -----------------------------------------------------------------------
   MIDI helpers
   ----------------------------------------------------------------------- */

typedef struct {
    LV2_Atom_Event event;
    uint8_t        msg[3];
} MIDIEvent;

static void sendNoteOn(Self* self, uint32_t frame, uint8_t pitch, uint8_t velocity,
                       uint8_t channel, uint32_t capacity)
{
    MIDIEvent ev;
    ev.event.time.frames   = frame;
    ev.event.body.type     = self->uris.midi_Event;
    ev.event.body.size     = 3;
    ev.msg[0] = (uint8_t)(0x90 | (channel & 0x0F));
    ev.msg[1] = pitch;
    ev.msg[2] = velocity;
    lv2_atom_sequence_append_event(self->midiOut, capacity, &ev.event);
}

static void sendNoteOff(Self* self, uint32_t frame, uint8_t pitch,
                        uint8_t channel, uint32_t capacity)
{
    MIDIEvent ev;
    ev.event.time.frames   = frame;
    ev.event.body.type     = self->uris.midi_Event;
    ev.event.body.size     = 3;
    ev.msg[0] = (uint8_t)(0x80 | (channel & 0x0F));
    ev.msg[1] = pitch;
    ev.msg[2] = 0;
    lv2_atom_sequence_append_event(self->midiOut, capacity, &ev.event);
}

/* -----------------------------------------------------------------------
   Beat-based playback
   ----------------------------------------------------------------------- */

/* Absolute beat = bar * beatsPerBar + barBeat (assumes constant time sig) */
static inline float absoluteBeat(int64_t bar, float barBeat, float beatsPerBar)
{
    return (float)bar * beatsPerBar + barBeat;
}

/* Fire note-offs for any active notes whose end beat has been passed. */
static void checkNoteOffs(Self* self, float upToBeat, uint32_t frame, uint32_t capacity)
{
    for (int i = self->numActiveNotes - 1; i >= 0; i--) {
        if (upToBeat >= self->activeNotes[i].endBeat) {
            sendNoteOff(self, frame,
                        self->activeNotes[i].pitch,
                        self->activeNotes[i].channel,
                        capacity);
            /* Remove by swapping with last */
            self->activeNotes[i] = self->activeNotes[--self->numActiveNotes];
        }
    }
}

static void addActiveNote(Self* self, uint8_t pitch, uint8_t channel, float endBeat)
{
    if (self->numActiveNotes >= MAX_ACTIVE_NOTES)
        return;
    self->activeNotes[self->numActiveNotes++] = (ActiveNote){
        .pitch   = pitch,
        .channel = channel,
        .endBeat = endBeat
    };
}

/* Find a pattern by id (linear search - fine for small timelines). */
static DspPattern* findPattern(DspTimeline* tl, int id)
{
    for (int i = 0; i < tl->numPatterns; i++)
        if (tl->patterns[i].id == id) return &tl->patterns[i];
    return NULL;
}

/*
 * Play notes whose trigger beat falls in [fromBeat, toBeat).
 * `frame` is the sample offset within the current process block to stamp
 * on outgoing MIDI events (approximate — uses frame of the time:Position
 * event that bracketed this range).
 */
static void playRange(Self* self, float fromBeat, float toBeat,
                      uint32_t frame, uint32_t capacity)
{
    if (toBeat <= fromBeat)
        return;

    checkNoteOffs(self, toBeat, frame, capacity);

    DspTimeline* tl = &self->timeline;

    for (int t = 0; t < tl->numTracks; t++) {
        DspTrack* track = &tl->tracks[t];

        for (int ii = 0; ii < track->numInstances; ii++) {
            DspPatternInstance* inst = &track->instances[ii];
            DspPattern* pat = findPattern(tl, inst->patternId);
            if (!pat) continue;

            /* Instance beat range (using first time sig — simplification) */
            float beatsPerBar = (tl->numTimeSigs > 0)
                ? (float)tl->timeSigs[0].beatsPerBar : 4.0f;
            float instStart = (float)inst->startBar * beatsPerBar;
            float instEnd   = instStart + inst->length * beatsPerBar;

            /* Skip if instance doesn't overlap our window */
            if (instEnd <= fromBeat || instStart >= toBeat)
                continue;

            for (int n = 0; n < pat->numNotes; n++) {
                DspNote* note = &pat->notes[n];

                /* Absolute beat when this note fires, accounting for pattern
                   looping within the instance. */
                float notePatBeat = (float)note->bar * beatsPerBar + note->beat
                                    - inst->startOffset;
                float patLen = pat->lengthBeats;

                /* How many full pattern cycles have occurred at fromBeat? */
                float relFrom = fromBeat - instStart;
                float relTo   = toBeat   - instStart;

                if (patLen <= 0.0f)
                    continue;

                /* First cycle index where this note could fall */
                int cycleStart = (relFrom > 0) ? (int)floorf(relFrom / patLen) : 0;
                int cycleEnd   = (int)floorf((relTo - 0.001f) / patLen);

                for (int c = cycleStart; c <= cycleEnd; c++) {
                    float noteFire = instStart + (float)c * patLen + notePatBeat;
                    if (noteFire < instStart || noteFire >= instEnd)
                        continue;
                    if (noteFire >= fromBeat && noteFire < toBeat) {
                        float noteEnd = noteFire + note->length;
                        sendNoteOn(self, frame, note->pitch, note->velocity,
                                   track->channel, capacity);
                        addActiveNote(self, note->pitch, track->channel, noteEnd);
                    }
                }
            }
        }
    }
}

/* -----------------------------------------------------------------------
   LV2 callbacks
   ----------------------------------------------------------------------- */

static void mapURIs(LV2_URID_Map* map, URIs* uris)
{
    uris->atom_Blank         = map->map(map->handle, LV2_ATOM__Blank);
    uris->atom_Object        = map->map(map->handle, LV2_ATOM__Object);
    uris->atom_Sequence      = map->map(map->handle, LV2_ATOM__Sequence);
    uris->atom_URID          = map->map(map->handle, LV2_ATOM__URID);
    uris->time_Position      = map->map(map->handle, LV2_TIME__Position);
    uris->time_bar           = map->map(map->handle, LV2_TIME__bar);
    uris->time_barBeat       = map->map(map->handle, LV2_TIME__barBeat);
    uris->time_beatsPerBar   = map->map(map->handle, LV2_TIME__beatsPerBar);
    uris->time_beatsPerMinute= map->map(map->handle, LV2_TIME__beatsPerMinute);
    uris->time_speed         = map->map(map->handle, LV2_TIME__speed);
    uris->midi_Event         = map->map(map->handle, LV2_MIDI__MidiEvent);
}

static void buildTestTimeline(DspTimeline* tl)
{
    tl->numTimeSigs = 1;
    tl->timeSigs    = &testTimeSig;
    tl->numBpms     = 1;
    tl->bpms        = &testBpm;
    tl->numPatterns = 1;
    tl->patterns    = &testPattern;
    tl->numTracks   = 1;
    tl->tracks      = &testTrack;
}

static LV2_Handle instantiate(
    const LV2_Descriptor* descriptor,
    double sampleRate,
    const char* bundlePath,
    const LV2_Feature* const* features)
{
    (void)descriptor; (void)bundlePath;

    Self* self = (Self*)calloc(1, sizeof(Self));
    if (!self)
        return NULL;

    const char* missing = lv2_features_query(
        features,
        LV2_LOG__log,  &self->logger.log, false,
        LV2_URID__map, &self->map,        true,
        NULL);

    lv2_log_logger_set_map(&self->logger, self->map);
    if (missing) {
        lv2_log_error(&self->logger, "Missing feature <%s>\n", missing);
        free(self);
        return NULL;
    }

    mapURIs(self->map, &self->uris);

    self->sampleRate  = sampleRate;
    self->speed       = 0.0f;
    self->beatsPerBar = 4.0f;
    self->bpm         = 120.0f;
    self->bar         = 0;
    self->barBeat     = 0.0f;

    buildTestTimeline(&self->timeline);

    return (LV2_Handle)self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data)
{
    Self* self = (Self*)instance;
    switch (port) {
        case PORT_CONTROL_IN: self->controlIn = (const LV2_Atom_Sequence*)data; break;
        case PORT_MIDI_OUT:   self->midiOut   = (LV2_Atom_Sequence*)data;       break;
    }
}

static void activate(LV2_Handle instance)
{
    Self* self = (Self*)instance;
    self->speed          = 0.0f;
    self->numActiveNotes = 0;
    self->bar            = 0;
    self->barBeat        = 0.0f;
}

static void run(LV2_Handle instance, uint32_t sample_count)
{
    Self* self = (Self*)instance;
    const URIs* uris = &self->uris;

    const uint32_t capacity = self->midiOut->atom.size;
    lv2_atom_sequence_clear(self->midiOut);
    self->midiOut->atom.type = uris->atom_Sequence;

    float prevBeat = absoluteBeat(self->bar, self->barBeat, self->beatsPerBar);
    uint32_t prevFrame = 0;

    LV2_ATOM_SEQUENCE_FOREACH(self->controlIn, ev) {
        uint32_t evFrame = (uint32_t)ev->time.frames;

        if (self->speed > 0.0f && evFrame > prevFrame)
            playRange(self, prevBeat, absoluteBeat(self->bar, self->barBeat, self->beatsPerBar)
                      + (float)(evFrame - prevFrame) * self->bpm / (60.0f * (float)self->sampleRate),
                      prevFrame, capacity);

        if (ev->body.type == uris->atom_Object || ev->body.type == uris->atom_Blank) {
            const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&ev->body;

            if (obj->body.otype == uris->time_Position) {
                const LV2_Atom_Long*  atomBar   = NULL;
                const LV2_Atom_Float* atomBeat  = NULL;
                const LV2_Atom_Float* atomBpb   = NULL;
                const LV2_Atom_Float* atomBpm   = NULL;
                const LV2_Atom_Float* atomSpeed = NULL;

                lv2_atom_object_get(obj,
                    uris->time_bar,            &atomBar,
                    uris->time_barBeat,        &atomBeat,
                    uris->time_beatsPerBar,    &atomBpb,
                    uris->time_beatsPerMinute, &atomBpm,
                    uris->time_speed,          &atomSpeed,
                    0);

                if (atomBar)   self->bar         = atomBar->body;
                if (atomBeat)  self->barBeat      = atomBeat->body;
                if (atomBpb)   self->beatsPerBar  = atomBpb->body;
                if (atomBpm)   self->bpm          = atomBpm->body;
                if (atomSpeed) {
                    float newSpeed = atomSpeed->body;
                    if (newSpeed == 0.0f && self->speed != 0.0f) {
                        /* Transport stopped — cancel all active notes */
                        for (int i = 0; i < self->numActiveNotes; i++)
                            sendNoteOff(self, evFrame,
                                        self->activeNotes[i].pitch,
                                        self->activeNotes[i].channel,
                                        capacity);
                        self->numActiveNotes = 0;
                    }
                    self->speed = newSpeed;
                }

                prevBeat  = absoluteBeat(self->bar, self->barBeat, self->beatsPerBar);
                prevFrame = evFrame;
            }
        }
    }

    /* Play remainder of block */
    if (self->speed > 0.0f) {
        float remaining = (float)(sample_count - prevFrame)
                          * self->bpm / (60.0f * (float)self->sampleRate);
        float endBeat = prevBeat + remaining;
        playRange(self, prevBeat, endBeat, prevFrame, capacity);

        /* Advance position for next block */
        self->barBeat += remaining;
        while (self->barBeat >= self->beatsPerBar) {
            self->barBeat -= self->beatsPerBar;
            self->bar++;
        }
    }
}

static void deactivate(LV2_Handle instance) { (void)instance; }

static void cleanup(LV2_Handle instance)
{
    free(instance);
}

static const void* extension_data(const char* uri)
{
    (void)uri;
    return NULL;
}

static const LV2_Descriptor descriptor = {
    LUVIE_DSP_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
    return index == 0 ? &descriptor : NULL;
}
