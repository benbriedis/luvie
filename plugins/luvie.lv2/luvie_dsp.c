#include "luvie_dsp.h"
#include "timeline_serial.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

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
   Timeline lifecycle
   ----------------------------------------------------------------------- */

static void freeDspTimeline(DspTimeline* tl)
{
    free(tl->timeSigs);
    free(tl->bpms);
    for (int i = 0; i < tl->numPatterns; i++)
        free(tl->patterns[i].notes);
    free(tl->patterns);
    for (int i = 0; i < tl->numTracks; i++)
        free(tl->tracks[i].instances);
    free(tl->tracks);
    memset(tl, 0, sizeof(*tl));
}

/* Returns 1 on success, 0 on invalid/truncated data.
   On success *out is a freshly-allocated timeline; caller owns it. */
static int deserializeTimeline(DspTimeline* out, const uint8_t* buf, uint32_t size)
{
    if (size < sizeof(TimelineHeader))
        return 0;

    const TimelineHeader* hdr = (const TimelineHeader*)buf;
    const uint8_t* p   = buf + sizeof(TimelineHeader);
    const uint8_t* end = buf + size;

    DspTimeline tl;
    memset(&tl, 0, sizeof(tl));

    /* Time signatures */
    if (hdr->numTimeSigs > 0) {
        size_t sz = hdr->numTimeSigs * sizeof(DspTimeSig);
        if (p + sz > end) goto fail;
        tl.timeSigs = (DspTimeSig*)malloc(sz);
        if (!tl.timeSigs) goto fail;
        memcpy(tl.timeSigs, p, sz);
        tl.numTimeSigs = (int)hdr->numTimeSigs;
        p += sz;
    }

    /* BPM markers */
    if (hdr->numBpms > 0) {
        size_t sz = hdr->numBpms * sizeof(DspBpmMarker);
        if (p + sz > end) goto fail;
        tl.bpms = (DspBpmMarker*)malloc(sz);
        if (!tl.bpms) goto fail;
        memcpy(tl.bpms, p, sz);
        tl.numBpms = (int)hdr->numBpms;
        p += sz;
    }

    /* Patterns */
    if (hdr->numPatterns > 0) {
        tl.patterns = (DspPattern*)calloc(hdr->numPatterns, sizeof(DspPattern));
        if (!tl.patterns) goto fail;
        tl.numPatterns = (int)hdr->numPatterns;

        for (uint32_t i = 0; i < hdr->numPatterns; i++) {
            if (p + sizeof(SerialPatternHeader) > end) goto fail;
            const SerialPatternHeader* sph = (const SerialPatternHeader*)p;
            p += sizeof(SerialPatternHeader);

            tl.patterns[i].id          = sph->id;
            tl.patterns[i].lengthBeats = sph->lengthBeats;
            tl.patterns[i].numNotes    = (int)sph->numNotes;

            if (sph->numNotes > 0) {
                size_t sz = sph->numNotes * sizeof(DspNote);
                if (p + sz > end) goto fail;
                tl.patterns[i].notes = (DspNote*)malloc(sz);
                if (!tl.patterns[i].notes) goto fail;
                memcpy(tl.patterns[i].notes, p, sz);
                p += sz;
            }
        }
    }

    /* Tracks */
    if (hdr->numTracks > 0) {
        tl.tracks = (DspTrack*)calloc(hdr->numTracks, sizeof(DspTrack));
        if (!tl.tracks) goto fail;
        tl.numTracks = (int)hdr->numTracks;

        for (uint32_t i = 0; i < hdr->numTracks; i++) {
            if (p + sizeof(SerialTrackHeader) > end) goto fail;
            const SerialTrackHeader* sth = (const SerialTrackHeader*)p;
            p += sizeof(SerialTrackHeader);

            tl.tracks[i].id           = sth->id;
            tl.tracks[i].channel      = sth->channel;
            tl.tracks[i].numInstances = (int)sth->numInstances;

            if (sth->numInstances > 0) {
                size_t sz = sth->numInstances * sizeof(DspPatternInstance);
                if (p + sz > end) goto fail;
                tl.tracks[i].instances = (DspPatternInstance*)malloc(sz);
                if (!tl.tracks[i].instances) goto fail;
                memcpy(tl.tracks[i].instances, p, sz);
                p += sz;
            }
        }
    }

    *out = tl;
    return 1;

fail:
    freeDspTimeline(&tl);
    return 0;
}

/* -----------------------------------------------------------------------
   Beat-based playback
   ----------------------------------------------------------------------- */

/* Absolute beat = bar * beatsPerBar + barBeat (assumes constant time sig) */
static inline float absoluteBeat(int64_t bar, float barBeat, float beatsPerBar)
{
    return (float)bar * beatsPerBar + barBeat;
}

/* Fire note-offs for active notes that end by upToBeat.
   Uses fromBeat + samplesPerBeat to assign each NoteOff its accurate frame. */
static void checkNoteOffs(Self* self, float upToBeat,
                           float fromBeat, uint32_t frameBase, float samplesPerBeat,
                           uint32_t capacity)
{
    for (int i = self->numActiveNotes - 1; i >= 0; i--) {
        if (upToBeat >= self->activeNotes[i].endBeat) {
            float beatOff = self->activeNotes[i].endBeat - fromBeat;
            uint32_t evFrame = (beatOff > 0.0f)
                ? frameBase + (uint32_t)(beatOff * samplesPerBeat)
                : frameBase;
            sendNoteOff(self, evFrame,
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
 */
static void playRange(Self* self, float fromBeat, float toBeat,
                      uint32_t frame, uint32_t capacity)
{
    if (toBeat <= fromBeat)
        return;

    const float samplesPerBeat = (float)self->sampleRate * 60.0f / self->bpm;
    checkNoteOffs(self, toBeat, fromBeat, frame, samplesPerBeat, capacity);

    DspTimeline* tl = &self->timeline;

    for (int t = 0; t < tl->numTracks; t++) {
        DspTrack* track = &tl->tracks[t];

        for (int ii = 0; ii < track->numInstances; ii++) {
            DspPatternInstance* inst = &track->instances[ii];
            DspPattern* pat = findPattern(tl, inst->patternId);
            if (!pat) continue;

            float beatsPerBar = (tl->numTimeSigs > 0)
                ? (float)tl->timeSigs[0].beatsPerBar : 4.0f;
            float instStart = inst->startBar * beatsPerBar;
            float instEnd   = instStart + inst->length * beatsPerBar;

            if (instEnd <= fromBeat || instStart >= toBeat)
                continue;

            for (int n = 0; n < pat->numNotes; n++) {
                DspNote* note = &pat->notes[n];

                /* Beat within pattern where this note fires (offset-adjusted) */
                float notePatBeat = note->beat - inst->startOffset;
                float patLen = pat->lengthBeats;
                if (patLen <= 0.0f)
                    continue;

                float relFrom = fromBeat - instStart;
                float relTo   = toBeat   - instStart;

                int cycleStart = (relFrom > 0) ? (int)floorf(relFrom / patLen) : 0;
                int cycleEnd   = (int)floorf(relTo / patLen);

                for (int c = cycleStart; c <= cycleEnd; c++) {
                    float noteFire = instStart + (float)c * patLen + notePatBeat;
                    if (noteFire < instStart || noteFire >= instEnd)
                        continue;
                    if (noteFire >= fromBeat && noteFire < toBeat) {
                        float noteEnd = noteFire + note->length;
                        uint32_t noteFrame = frame
                            + (uint32_t)((noteFire - fromBeat) * samplesPerBeat);
                        sendNoteOn(self, noteFrame, note->pitch, note->velocity,
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
    uris->atom_Chunk         = map->map(map->handle, LV2_ATOM__Chunk);
    uris->time_Position      = map->map(map->handle, LV2_TIME__Position);
    uris->time_bar           = map->map(map->handle, LV2_TIME__bar);
    uris->time_barBeat       = map->map(map->handle, LV2_TIME__barBeat);
    uris->time_beatsPerBar   = map->map(map->handle, LV2_TIME__beatsPerBar);
    uris->time_beatsPerMinute= map->map(map->handle, LV2_TIME__beatsPerMinute);
    uris->time_speed         = map->map(map->handle, LV2_TIME__speed);
    uris->midi_Event         = map->map(map->handle, LV2_MIDI__MidiEvent);
    uris->luvie_timeline     = map->map(map->handle, LUVIE_TIMELINE_URI);
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
    lv2_atom_forge_init(&self->forge, self->map);

    self->sampleRate  = sampleRate;
    self->speed       = 0.0f;
    self->beatsPerBar = 4.0f;
    self->bpm         = 120.0f;
    self->bar         = 0;
    self->barBeat     = 0.0f;

    /* timeline starts empty — will be populated by first UI message */

    return (LV2_Handle)self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data)
{
    Self* self = (Self*)instance;
    switch (port) {
        case PORT_CONTROL_IN: self->controlIn  = (const LV2_Atom_Sequence*)data; break;
        case PORT_MIDI_OUT:   self->midiOut    = (LV2_Atom_Sequence*)data;       break;
        case PORT_NOTIFY_OUT: self->notifyOut  = (LV2_Atom_Sequence*)data;       break;
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

    /* Set up forge to write into notifyOut (port may not be connected) */
    const int hasNotify = (self->notifyOut != NULL);
    LV2_Atom_Forge_Frame notifyFrame;
    if (hasNotify) {
        const uint32_t notifyCapacity = self->notifyOut->atom.size;
        lv2_atom_sequence_clear(self->notifyOut);
        self->notifyOut->atom.type = uris->atom_Sequence;
        lv2_atom_forge_set_buffer(&self->forge,
                                  (uint8_t*)self->notifyOut,
                                  sizeof(LV2_Atom) + notifyCapacity);
        lv2_atom_forge_sequence_head(&self->forge, &notifyFrame, 0);
    }

    float prevBeat = absoluteBeat(self->bar, self->barBeat, self->beatsPerBar);
    uint32_t prevFrame = 0;

    LV2_ATOM_SEQUENCE_FOREACH(self->controlIn, ev) {
        uint32_t evFrame = (uint32_t)ev->time.frames;

        /* --- Timeline update from UI --- */
        if (ev->body.type == uris->luvie_timeline) {
            /* Play any pending audio up to this point first */
            if (self->speed > 0.0f && evFrame > prevFrame) {
                float thisBeat = prevBeat + (float)(evFrame - prevFrame)
                                 * self->bpm / (60.0f * (float)self->sampleRate);
                playRange(self, prevBeat, thisBeat, prevFrame, capacity);
                prevBeat  = thisBeat;
                prevFrame = evFrame;
            }

            /* Cancel all active notes */
            for (int i = 0; i < self->numActiveNotes; i++)
                sendNoteOff(self, evFrame,
                            self->activeNotes[i].pitch,
                            self->activeNotes[i].channel,
                            capacity);
            self->numActiveNotes = 0;

            /* Deserialize new timeline */
            const uint8_t* data = (const uint8_t*)LV2_ATOM_BODY(&ev->body);
            DspTimeline newTl;
            if (deserializeTimeline(&newTl, data, ev->body.size)) {
                freeDspTimeline(&self->timeline);
                self->timeline = newTl;
            }
            continue;
        }

        /* --- Time position from host --- */
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

                /* Forward time:Position to the UI */
                if (hasNotify) {
                    lv2_atom_forge_frame_time(&self->forge, ev->time.frames);
                    lv2_atom_forge_write(&self->forge, &ev->body,
                                         sizeof(LV2_Atom) + ev->body.size);
                }
            }
        }
    }

    if (hasNotify)
        lv2_atom_forge_pop(&self->forge, &notifyFrame);

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
    Self* self = (Self*)instance;
    freeDspTimeline(&self->timeline);
    free(self);
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
