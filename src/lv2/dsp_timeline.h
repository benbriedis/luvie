#ifndef DSP_TIMELINE_H
#define DSP_TIMELINE_H

#include <stdint.h>

/* Simplified timeline data structures for the realtime DSP plugin.
   All pitches are absolute MIDI note numbers (0-127).
   Beat positions are floating-point, bar-relative. */

typedef struct {
    float   beat;      /* beat within pattern (absolute, 0-indexed) */
    float   length;    /* duration in beats */
    uint8_t pitch;     /* absolute MIDI note number */
    uint8_t velocity;
    uint8_t _pad[2];
} DspNote;

typedef struct {
    int      id;
    float    lengthBeats;
    int      numNotes;
    DspNote* notes;
} DspPattern;

typedef struct {
    int   id;
    int   patternId;
    float startBar;
    float length;       /* in bars */
    float startOffset;  /* beat offset into pattern at instance start */
} DspPatternInstance;

typedef struct {
    int                  id;
    uint8_t              channel;
    int                  numInstances;
    DspPatternInstance*  instances;
} DspTrack;

typedef struct {
    int         bar;
    int         beatsPerBar;   /* numerator */
    int         beatUnit;      /* denominator (unused for now) */
} DspTimeSig;

typedef struct {
    int   bar;
    float bpm;
} DspBpmMarker;

typedef struct {
    int           numTimeSigs;
    DspTimeSig*   timeSigs;
    int           numBpms;
    DspBpmMarker* bpms;
    int           numPatterns;
    DspPattern*   patterns;
    int           numTracks;
    DspTrack*     tracks;
} DspTimeline;

#endif /* DSP_TIMELINE_H */
