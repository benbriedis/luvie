#include "uris.h"

#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/core/lv2_util.h>
#include <lv2/log/log.h>
#include <lv2/log/logger.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum { FIFTHS_IN = 0, FIFTHS_OUT = 1 };

typedef struct {
  // Features
  LV2_URID_Map*  map;
  LV2_Log_Logger logger;

  // Ports
  const LV2_Atom_Sequence* in_port;
  LV2_Atom_Sequence*       out_port;

  // URIs
  FifthsURIs uris;
} Fifths;

static void
connect_port(LV2_Handle instance, uint32_t port, void* data)
{
printf("In fifths connect_port()\n");	
  Fifths* self = (Fifths*)instance;
  switch (port) {
  case FIFTHS_IN:
    self->in_port = (const LV2_Atom_Sequence*)data;
	//self->control_port_in = (LV2_Atom_Sequence*)data;
    break;
  case FIFTHS_OUT:
    self->out_port = (LV2_Atom_Sequence*)data;
    break;
  default:
    break;
  }
}

static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    rate,
            const char*               path,
            const LV2_Feature* const* features)
{
  // Allocate and initialise instance structure.
  Fifths* self = (Fifths*)calloc(1, sizeof(Fifths));
  if (!self) {
    return NULL;
  }

  // Scan host features for URID map
  // clang-format off
  const char*  missing = lv2_features_query(
    features,
    LV2_LOG__log,  &self->logger.log, false,
    LV2_URID__map, &self->map,        true,
    NULL);
  // clang-format on

  lv2_log_logger_set_map(&self->logger, self->map);
  if (missing) {
    lv2_log_error(&self->logger, "Missing feature <%s>\n", missing);
    free(self);
    return NULL;
  }

  map_fifths_uris(self->map, &self->uris);

  return (LV2_Handle)self;
}

static void
cleanup(LV2_Handle instance)
{
  free(instance);
}

static void
run(LV2_Handle instance, uint32_t sample_count)
{
  Fifths*           self = (Fifths*)instance;
//  const FifthsURIs* uris = &self->uris;

  // Struct for a 3 byte MIDI event, used for writing notes
  typedef struct {
    LV2_Atom_Event event;
    uint8_t        msg[3];
  } MIDINoteEvent;


  	/* Initially self->out_port contains a Chunk with size set to capacity */
	const uint32_t out_capacity = self->out_port->atom.size;

	/* Write an empty Sequence header to the output */
	lv2_atom_sequence_clear(self->out_port);
	self->out_port->atom.type = self->uris.atom_Sequence;

			MIDINoteEvent note;
//			note.event.time.frames = 0;
			note.event.time.frames = 100;
			note.event.body.type =  self->uris.midi_Event;
			note.event.body.size = 3;
			note.msg[0] = 0x80; 	// Note off
			note.msg[1] = 60;
			note.msg[2] = 0;
			lv2_atom_sequence_append_event(self->out_port, out_capacity, &note.event);


printf("run()  SENT A NOTE\n");	

}

static const void*
extension_data(const char* uri)
{
  return NULL;
}

static const LV2_Descriptor descriptor = {EG_FIFTHS_URI,
                                          instantiate,
                                          connect_port,
                                          NULL, // activate,
                                          run,
                                          NULL, // deactivate,
                                          cleanup,
                                          extension_data};

LV2_SYMBOL_EXPORT const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
  return index == 0 ? &descriptor : NULL;
}

