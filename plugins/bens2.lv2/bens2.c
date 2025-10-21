#include <lv2/core/lv2_util.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/log/logger.h>
#include <lv2/urid/urid.h>
#include <lv2/core/lv2.h>
#include <lv2/time/time.h>
#include <lv2/midi/midi.h>
#include <lv2/patch/patch.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#define PLUGIN_URI "http://benbriedis.com/lv2/bens2"

/*
	GOAL: to play a simple hard-coded arpeggio in MIDI, using timing coming from the host.

	LATER: 
	We don't want the pattern to supplied in real time - so we need the host to 
	supply us with these patterns during initialisation OR load them ourselves 
	during initialisation. Then we can play MULTIPLE patterns through the one plugin
	they will need their own start and end points. 

	QUERY: 
	whether we can have a 'peer' plugin supply us with commands via the host or 
	whether we need our own host.
*/

typedef struct {
	LV2_URID atom_Blank;
	LV2_URID atom_Float;
	LV2_URID atom_Object;
	LV2_URID atom_Path;
	LV2_URID atom_Resource;
	LV2_URID atom_Sequence;
	LV2_URID atom_URID;
	LV2_URID atom_eventTransfer;
	LV2_URID time_Position;
	LV2_URID time_barBeat;
	LV2_URID time_beatsPerMinute;
	LV2_URID time_speed;
	LV2_URID midi_Event;
	LV2_URID patch_Set;
	LV2_URID patch_property;
	LV2_URID patch_value;
} URIs;


enum { METRO_CONTROL = 0, MIDI_OUT = 1 };

/** During execution this plugin can be in one of 3 states: */
typedef enum {
	STATE_ON,
	STATE_OFF,
} State;

/*
   All data associated with a plugin instance is stored here.
   Port buffers.
*/
typedef struct {
	LV2_URID_Map* map;
	LV2_Log_Logger logger;

	/* Output MIDI port: */
	LV2_Atom_Sequence* midi_port_out;

	URIs uris;   // Cache of mapped URIDs

	struct {
		LV2_Atom_Sequence* control;
		float* output;
	} ports;

	// Variables to keep track of the tempo information sent by the host
	double rate;  // Sample rate
	float bpm;   // Beats per minute (tempo)
	float speed; // Transport speed (usually 0=stop, 1=play)

	State state;
	uint32_t noteLen;
//XXX we're going to need a bunch of state storing where we are in patterns... eg

	uint32_t elapsedLen; // Frames since the start of the last click
} Self;

/*
   The host passes the plugin descriptor, sample rate, and bundle
   path for plugins that need to load additional resources (e.g. waveforms).
   The features parameter contains host-provided features defined in LV2
   extensions, but this simple plugin does not use any.

   This function is in the 'instantiation' threading class, so no other
   methods on this instance will be called concurrently with it.
*/
static LV2_Handle instantiate(
	const LV2_Descriptor* pluginDescriptor,
	double sampleRate,
	const char* bundlePath,  //XXX file path I think for extra resources
	const LV2_Feature* const* features     //XXX host provided features
)
{
	Self* self = (Self*)calloc(1, sizeof(Self));
	if (!self)
		return NULL;

	/* Scan host features for URID map */
	const char* missing = lv2_features_query(
		features,
		LV2_LOG__log, &self->logger.log, false,
		LV2_URID__map, &self->map, true,
		NULL
	);

	lv2_log_logger_set_map(&self->logger, self->map);
	if (missing) {
		lv2_log_error(&self->logger, "Missing feature <%s>\n", missing);
		free(self);
		return NULL;
	}

	// Map URIS
	URIs* const uris = &self->uris;
	LV2_URID_Map* const map = self->map;

	uris->atom_Blank          = map->map(map->handle, LV2_ATOM__Blank);
	uris->atom_Float          = map->map(map->handle, LV2_ATOM__Float);
	uris->atom_Object         = map->map(map->handle, LV2_ATOM__Object);
	uris->atom_Path           = map->map(map->handle, LV2_ATOM__Path);
	uris->atom_Resource       = map->map(map->handle, LV2_ATOM__Resource);
	uris->atom_Sequence       = map->map(map->handle, LV2_ATOM__Sequence);
	uris->atom_URID           = map->map(map->handle, LV2_ATOM__URID);
	uris->atom_eventTransfer  = map->map(map->handle, LV2_ATOM__eventTransfer);
	uris->midi_Event          = map->map(map->handle, LV2_MIDI__MidiEvent);
	uris->patch_Set           = map->map(map->handle, LV2_PATCH__Set);
	uris->patch_property      = map->map(map->handle, LV2_PATCH__property);
	uris->patch_value         = map->map(map->handle, LV2_PATCH__value);
	uris->time_Position       = map->map(map->handle, LV2_TIME__Position);
	uris->time_barBeat        = map->map(map->handle, LV2_TIME__barBeat);
	uris->time_beatsPerMinute = map->map(map->handle, LV2_TIME__beatsPerMinute);
	uris->time_speed          = map->map(map->handle, LV2_TIME__speed);

	/* Initialise instance data: */
	self->rate = sampleRate;
	self->bpm = 120.0f;
	self->noteLen = (uint32_t)(0.3 * sampleRate);
	self->state = STATE_OFF;

	return (LV2_Handle)self;
}

/**
   Play back audio for the range [begin..end) relative to this cycle.  This is
   called by run() in-between events to output audio up until the current time.
*/
static void play(Self* self, uint32_t begin, uint32_t end)
{
	typedef struct {
		LV2_Atom_Event event;
		uint8_t msg[3];
	} MIDINoteEvent;

	MIDINoteEvent note;

	const uint32_t out_capacity = self->midi_port_out->atom.size;

	//float* const output = self->ports.output;
	const uint32_t framesPerBeat = (uint32_t)(60.0f / self->bpm * self->rate);

	if (self->speed == 0.0f) {
		//memset(output, 0, (end - begin) * sizeof(float));
		return;
	}

	for (uint32_t i = begin; i < end; ++i) {
		switch (self->state) {
			case STATE_ON:
				if (self->elapsedLen >= self->noteLen) {
//TODO can we use logger for this?					
printf("NOTE FINISHED\n");

					note.event.time.frames = 0;
					note.event.body.type = 47;
					note.event.body.size = 3;
					note.msg[0] = 0x90;		// Note On
					note.msg[1] = 60; 		// Middle C
					note.msg[2] = 100;		// Velocity

//					lv2_atom_sequence_append_event(self->out_port, out_capacity, &note.event);
					lv2_atom_sequence_append_event(self->midi_port_out, sizeof(MIDINoteEvent), &note.event);

					self->state = STATE_OFF;
				}
				break;
			case STATE_OFF:
//				output[i] = 0.0f;
				break;
		}

		// Update elapsed time and start attack if necessary
		if (++self->elapsedLen == framesPerBeat) {
printf("NOTE STARTING\n");
			note.event.time.frames = 0;
			note.event.body.type = 47;
			note.event.body.size = 3;
			note.msg[0] = 0x80; 	// Note off
			note.msg[1] = 60;
			note.msg[2] = 0;
//			lv2_atom_sequence_append_event(self->out_port, out_capacity, &note.event);
			lv2_atom_sequence_append_event(self->midi_port_out, sizeof(MIDINoteEvent), &note.event);

			self->state = STATE_ON;
			self->elapsedLen = 0;
		}
	}
}

/*
   The plugin must store the data location, but data may not be accessed except in run().
   This method is in the 'audio' threading class, and is called in the same context as run().
*/
static void connect_port(LV2_Handle instance, uint32_t port, void* data)
{
	Self* self = (Self*)instance;

	switch (port) {
		case METRO_CONTROL:
			self->control = (LV2_Atom_Sequence*)data;
			break;

		case MIDI_OUT:
			self->midi_port_out = (LV2_Atom_Sequence*)data;
			break;
	}
}

/* The plugin must reset all internal state */
static void activate(LV2_Handle instance)
{
	Self* self = (Self*)instance;

	self->elapsedLen = 0;
	self->state = STATE_OFF;
}

/*
   Update the current position based on a host message.  This is called by
   run() when a time:Position is received.
*/
static void updatePosition(Self* self, const LV2_Atom_Object* obj)
{
	const URIs* uris = &self->uris;

	LV2_Atom* beat = NULL;
	LV2_Atom* bpm = NULL;
	LV2_Atom* speed = NULL;

	lv2_atom_object_get(obj,
		uris->time_barBeat, &beat,
		uris->time_beatsPerMinute, &bpm,
		uris->time_speed, &speed,
		NULL
	);

    /* Tempo changed */
	if (bpm && bpm->type == uris->atom_Float)
		self->bpm = ((LV2_Atom_Float*)bpm)->body;

	/* Speed changed, e.g. 0 (stop) to 1 (play) */
	if (speed && speed->type == uris->atom_Float) 
		self->speed = ((LV2_Atom_Float*)speed)->body;

    // Received a beat position, synchronise
    // This hard sync may cause clicks, a real plugin would be more graceful

	if (beat && beat->type == uris->atom_Float) {
		const float frames_per_beat = (float)(60.0 / self->bpm * self->rate);
		const float bar_beats       = ((LV2_Atom_Float*)beat)->body;
		const float beat_beats      = bar_beats - floorf(bar_beats);
		self->elapsedLen           = (uint32_t)(beat_beats * frames_per_beat);

		if (self->elapsedLen < self->noteLen)
			self->state = STATE_ON;
		else 
			self->state = STATE_OFF;
	}
}

/*
	The `run()` method is the main process function of the plugin.
	It processes a block of audio in the audio context.  
	Since this plugin is `lv2:hardRTCapable`, `run()` must be real-time safe, 
	so blocking (e.g. with a mutex) or memory allocation are not allowed.
*/
static void run(LV2_Handle instance, uint32_t sample_count)
{
	Self* self = (Self*)instance;
 	const URIs* uris = &self->uris;

	const LV2_Atom_Sequence* in = self->control;
	uint32_t last_t = 0;

	/* Write an empty Sequence header to the output */
	lv2_atom_sequence_clear(self->midi_port_out);

//FIXME...	
//	self->out_port->atom.type = self->in_port->atom.type;
//XXX fifths has a value of 15...	
//self->out_port->atom.type = 15;

//TODO try using:
//  LV2_ATOM_SEQUENCE_FOREACH (self->in_port, ev) {

	/* Loop through events: */
	for (const LV2_Atom_Event* ev = lv2_atom_sequence_begin(&in->body);
       !lv2_atom_sequence_is_end(&in->body, in->atom.size, ev);
       ev = lv2_atom_sequence_next(ev)) 
	{
		// Play the click for the time slice from last_t until now
		play(self, last_t, (uint32_t)ev->time.frames);

		//TODO deprecated Blank to tolerate old hosts (DELETE Blank? How deprecated is it?)
		if (ev->body.type == uris->atom_Object || ev->body.type == uris->atom_Blank) {
			const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&ev->body;
			if (obj->body.otype == uris->time_Position) 
				// Received position information, update
				updatePosition(self, obj);
		}

		last_t = (uint32_t)ev->time.frames;
  }

	/* Play out the remainder of cycle: */
	play(self, last_t, sample_count);
}

/*
   The host will not call `run()` again until another call to `activate()`.
   It is mainly useful for more advanced plugins with 'live' characteristics such as those with
   auxiliary processing threads.

   This method is in the ``instantiation'' threading class, so no other
   methods on this instance will be called concurrently with it.
*/
static void deactivate(LV2_Handle instance)
{}

/*
   Destroy a plugin instance (counterpart to `instantiate()`).

   This method is in the ``instantiation'' threading class, so no other
   methods on this instance will be called concurrently with it.
*/
static void cleanup(LV2_Handle instance)
{
	free(instance);
}

/*
   This function returns any extension data supported by the plugin.
   It is not an instance method, but a function on the plugin descriptor.  
   It is usually used by plugins to implement additional interfaces.  

   This method is in the 'discovery' threading class, so no other functions
   or methods in this plugin library will be called concurrently with it.
*/
static const void* extension_data(const char* uri)
{
	return NULL;
}

/* Every plugin must define an `LV2_Descriptor`.  It is best to define descriptors statically. */
static const LV2_Descriptor descriptor = {PLUGIN_URI, instantiate, connect_port, activate, 
	run, deactivate, cleanup, extension_data};

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
	return index == 0 ? &descriptor : NULL;
}

