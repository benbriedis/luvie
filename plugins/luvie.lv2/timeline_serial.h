#ifndef TIMELINE_SERIAL_H
#define TIMELINE_SERIAL_H

/* Flat binary format for UI → DSP timeline updates.
   All values are native-endian (both ends run on the same machine).

   Layout:
     TimelineHeader
     DspTimeSig      [numTimeSigs]
     DspBpmMarker    [numBpms]
     for each pattern:
       SerialPatternHeader
       DspNote        [numNotes]
     for each track:
       SerialTrackHeader
       DspPatternInstance [numInstances]
*/

#include "dsp_timeline.h"
#include <stdint.h>

#define LUVIE_TIMELINE_URI "https://github.com/benbriedis/luvie#TimelineUpdate"

typedef struct {
    uint32_t numTimeSigs;
    uint32_t numBpms;
    uint32_t numPatterns;
    uint32_t numTracks;
} TimelineHeader;

typedef struct {
    int32_t  id;
    float    lengthBeats;
    uint32_t numNotes;
} SerialPatternHeader;

typedef struct {
    int32_t  id;
    uint8_t  channel;
    uint8_t  _pad[3];
    uint32_t numInstances;
} SerialTrackHeader;

#endif /* TIMELINE_SERIAL_H */
