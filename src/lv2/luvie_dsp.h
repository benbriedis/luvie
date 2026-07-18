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

/* The UI sends the project JSON to control_in as one or more `luvie_state` atoms.
   Each atom body is this fixed 16-byte header followed by `chunkSize` JSON bytes, so
   a session larger than a single host buffer (atom port or worker ring) can be split
   across several atoms and reassembled on the worker thread. All four fields are
   uint32_t, giving a naturally-packed header with no padding.

   Every payload the DSP hands to the LV2 Worker starts with this header, so msgId
   doubles as the worker's message discriminator: msgId 0 is reserved and means the
   body is not JSON at all but a LuvieLoopState (see below). Real state sends number
   from 1 upwards.

     msgId      identifies one complete message; it changes for every full send, so a
                dropped chunk is detected when the next message starts (offset 0).
                0 is reserved for non-state worker messages.
     totalSize  total JSON byte count across all chunks of this message.
     offset     byte offset of this chunk within the JSON.
     chunkSize  JSON bytes in this chunk (== atom body size - sizeof(LuvieStateChunk)). */
typedef struct {
    uint32_t msgId;
    uint32_t totalSize;
    uint32_t offset;
    uint32_t chunkSize;
} LuvieStateChunk;

/* Loop state (`luvie_loop` atoms on control_in). Loop mode and the active-loop set
   are runtime state, not project data, so they never travel in the JSON blob; the
   UI ships them separately whenever they change. They still have to be applied off
   the audio thread (applying rebuilds the Sequencer snapshot, which allocates), so
   like state they go via the LV2 Worker. The payload is small and bounded by the
   pattern count, so it is always one atom — no chunking.

   Atom body: a LuvieStateChunk header with msgId 0 (the marker that says "loop
   message, not JSON"), then this header, then `count` LuvieLoopEntry records. */
typedef struct {
    uint32_t loopMode;   /* 1 while the UI is in Loop Mode, 0 in Song Mode */
    uint32_t count;      /* number of LuvieLoopEntry records that follow */
} LuvieLoopState;

/* One pattern's loop state. A pattern appears here if it is active, manual, or
   manually disabled; anchorBar is meaningful only when LUVIE_LOOP_ACTIVE is set. */
enum {
    LUVIE_LOOP_ACTIVE   = 1u << 0,  /* in LoopManager::patterns() */
    LUVIE_LOOP_MANUAL   = 1u << 1,  /* turned on by a Loop-Editor switch */
    LUVIE_LOOP_DISABLED = 1u << 2   /* silenced by a Loop-Editor switch */
};
typedef struct {
    int32_t  patternId;
    float    anchorBar;
    uint32_t flags;
} LuvieLoopEntry;

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
    LV2_URID luvie_midi;          /* one-shot audition MIDI (raw bytes), UI -> DSP */
    LV2_URID luvie_loop;          /* loop mode + active loop set, UI -> DSP */
    LV2_URID state_StateChanged;  /* notify host that state is dirty */
} URIs;

#endif /* LUVIE_DSP_H */
