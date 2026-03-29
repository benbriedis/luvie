#ifndef LUVIE_DSP_H
#define LUVIE_DSP_H

#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/core/lv2_util.h>
#include <lv2/log/logger.h>
#include <lv2/midi/midi.h>
#include <lv2/time/time.h>
#include <lv2/urid/urid.h>
#include <stdint.h>
#include "dsp_timeline.h"

#define LUVIE_DSP_URI "https://github.com/benbriedis/luvie/luvie_dsp"

/* Port indices */
enum {
    PORT_CONTROL_IN  = 0,
    PORT_MIDI_OUT    = 1,
    PORT_NOTIFY_OUT  = 2
};

typedef struct {
    LV2_URID atom_Blank;
    LV2_URID atom_Object;
    LV2_URID atom_Sequence;
    LV2_URID atom_URID;
    LV2_URID time_Position;
    LV2_URID time_bar;
    LV2_URID time_barBeat;
    LV2_URID time_beatsPerBar;
    LV2_URID time_beatsPerMinute;
    LV2_URID time_speed;
    LV2_URID midi_Event;
    LV2_URID atom_Chunk;
    LV2_URID luvie_timeline;
} URIs;

/* Active note being played (awaiting note-off) */
typedef struct {
    uint8_t pitch;
    uint8_t channel;
    float   endBeat;   /* absolute beat when note-off is due */
} ActiveNote;

#define MAX_ACTIVE_NOTES 64

typedef struct {
    LV2_URID_Map*  map;
    LV2_Log_Logger logger;
    LV2_Atom_Forge forge;

    /* Ports */
    const LV2_Atom_Sequence* controlIn;
    LV2_Atom_Sequence*       midiOut;
    LV2_Atom_Sequence*       notifyOut;

    URIs uris;

    double  sampleRate;
    float   speed;         /* 0=stopped, 1=playing */
    int64_t bar;           /* current bar (from host) */
    float   barBeat;       /* beat within current bar (from host) */
    float   beatsPerBar;   /* time signature numerator */
    float   bpm;

    DspTimeline timeline;

    ActiveNote activeNotes[MAX_ACTIVE_NOTES];
    int        numActiveNotes;
} Self;

#endif /* LUVIE_DSP_H */
