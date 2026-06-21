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
#include <stdbool.h>
#include <stdint.h>

#define LUVIE_DSP_URI "https://github.com/benbriedis/luvie/luvie_dsp"

/* Port indices.

   A single atom output port carries everything: MIDI events (for the host to
   route to the instrument), plus a time:Position (for the UI playhead) and
   state:StateChanged (for the host). One output port — like a normal MIDI
   generator — is what hosts (Ardour) reliably route; a separate notify/atom
   output sitting ahead of the MIDI port confused Ardour's MIDI routing. */
enum {
    PORT_CONTROL_IN = 0,
    PORT_OUT        = 1
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
    LV2_URID time_frame;          /* sample-frame transport position */
    LV2_URID atom_Long;
    LV2_URID atom_Int;
    LV2_URID atom_Float;
    LV2_URID midi_MidiEvent;      /* MIDI event atom type (for midi_out) */
    LV2_URID atom_Chunk;
    LV2_URID luvie_state;         /* full JSON state blob */
    LV2_URID state_StateChanged;  /* notify host that state is dirty */
} URIs;

#endif /* LUVIE_DSP_H */
