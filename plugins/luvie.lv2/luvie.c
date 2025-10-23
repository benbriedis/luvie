
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/core/lv2_util.h>
#include <lv2/log/logger.h>
#include <lv2/midi/midi.h>
#include <lv2/patch/patch.h>
#include <lv2/time/time.h>
#include <lv2/urid/urid.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#define PLUGIN_URI "http://benbriedis.com/lv2/luvie"

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


enum { CONTROL_IN = 0, MIDI_OUT = 1 };

typedef enum {
	NOTE_ON,
	NOTE_OFF,
} State;

/*
   All data associated with a plugin instance is stored here.
   Port buffers.
*/
typedef struct {
	LV2_URID_Map* map;
	LV2_Log_Logger logger;

	/* Ports: */
	const LV2_Atom_Sequence* control_port_in;
	LV2_Atom_Sequence* midi_port_out;

	URIs uris;   // Cache of mapped URIDs

	// Variables to keep track of the tempo information sent by the host
	double sampleRate; 
	float bpm;
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
	self->sampleRate = sampleRate;
	self->bpm = 120.0f;
	self->noteLen = (uint32_t)(0.3 * sampleRate);
	self->state = NOTE_OFF;

	return (LV2_Handle)self;
}

//TODO move these patterns into self

typedef struct {
	float start;
	float pitch;
	float length;
	float velocity;

	int state; //XXX probably wont need long term
} Note;

typedef struct {
    char name[20];
    int length;
    int baseOctave;
    int baseNote;
	Note notes[5];
	//TODO probably add a MIDI channel here

	uint32_t elapsed; // in frames
} Pattern;

//XXX cf guaranteeing that the notes appear in order. Then can maintain a 'next note' for each pattern (maybe not worthwhile though...)
Pattern patterns[] = {{
	"pattern1", 4, 3, 0, {
		{0.0, 0, 0.5, 100, NOTE_OFF, 0},
		{0.0, 7, 0.5, 100, NOTE_OFF, 0},
		{1.0, 5, 0.5, 100, NOTE_OFF, 0},
		{2.0, 7, 0.5, 100, NOTE_OFF, 0},
		{3.0, 9, 0.5, 100, NOTE_OFF, 0},
	}
}, {
	"pattern2", 4, 3, 0, {
		{0.0, 8, 0.5, 100, NOTE_OFF, 0},
		{0.0, 7, 0.5, 100, NOTE_OFF, 0},
		{1.0, 5, 0.5, 100, NOTE_OFF, 0},
		{2.0, 7, 0.5, 100, NOTE_OFF, 0},
		{3.0, 7, 0.5, 100, NOTE_OFF, 0},
	}
}};

/*XXX JSON-style:
const patterns = [
	{
		name: 'pattern1',	//XXX or make a key. Or store as a key when loaded in. Could be an object or something
		length: 4,
		baseOctave: 3,  //XXX not sure
		baseNote: 0,    //XXX maybe C? MAYBE combine with baseOctave
		notes = [
			{start: 0.0, pitch: 0, length:0.5, velocity:100},
			{start: 0.0, pitch: 7, length:0.5, velocity:100},
			{start: 1.0, pitch: 5, length:0.5, velocity:100},
			{start: 2.0, pitch: 7, length:0.5, velocity:100},
			{start: 3.0, pitch: 9, length:0.5, velocity:100},
		]
	},
	{
		name: 'pattern2',
		length: 4,
		baseOctave: 3, 
		baseNote: 0,  
		notes = [
			{start: 0.0, pitch: 8, length:0.5, velocity:100},
			{start: 0.0, pitch: 7, length:0.5, velocity:100},
			{start: 1.0, pitch: 5, length:0.5, velocity:100},
			{start: 2.0, pitch: 7, length:0.5, velocity:100},
			{start: 3.0, pitch: 7, length:0.5, velocity:100},
		]
	},
];
*/


/**
   Play back audio for the range [begin..end) relative to this cycle.  This is
   called by run() in-between events to output audio up until the current time.
*/
static void play(Self* self, uint32_t begin, uint32_t end, uint32_t outCapacity)
{
	typedef struct {
		LV2_Atom_Event event;
		uint8_t msg[3];
	} MIDINoteEvent;

	const uint32_t framesPerBeat = (uint32_t)(60.0f / self->bpm * self->sampleRate);

//TODO replace 'speed' with 'playing' or something
	if (self->speed == 0.0f) 
		return;

//TODO can we use logger? 

//TODO presumbly could cut this up into segments for a more efficient solution.
//     Could/should create an index that contains all note start and end offsets in order.
//     Store a 'last position' and go from there each time. The states can be stored with the patterns.

	MIDINoteEvent out;

	for (uint32_t i = begin; i < end; ++i) {
		Pattern* pattern = &patterns[0];

		int patternEnd = pattern->length * framesPerBeat; 

		for (int j=0; j<5; j++) {  //FIXME
			Note* note = &pattern->notes[j]; 

			int noteStart = note->start * framesPerBeat;
			int noteLen = note->length * framesPerBeat;
			int noteEnd = noteStart + noteLen;

			int pitch = pattern->baseOctave * 12 + pattern->baseNote + note->pitch;

			if (note->state == NOTE_OFF && pattern->elapsed >= noteStart && pattern->elapsed < noteEnd) {
				note->state = NOTE_ON;

				out.event.time.frames = 0;   //FIXME can we use frames here or some other mechanism to place the note accurately within the frame?			
				out.event.body.type =  self->uris.midi_Event;
				out.event.body.size = 3;
				out.msg[0] = 0x90;		// Note On for channel 0
				out.msg[1] = pitch;
				out.msg[2] = 100;		// Velocity

				lv2_atom_sequence_append_event(self->midi_port_out, outCapacity, &out.event);
			}

			else if (note->state == NOTE_ON && pattern->elapsed == noteEnd - 1) {
				note->state = NOTE_OFF;

				out.event.time.frames = 0;    //FIXME reckon this is the desired offset. May need to graph out to be sure (should do this anyway)
				out.event.body.type = self->uris.midi_Event;
				out.event.body.size = 3;
				out.msg[0] = 0x80; 	// Note off
				out.msg[1] = pitch;       
				out.msg[2] = 0;       //XXX this is how fast to release note (127 fastest). Not always (often?) implemented?

				lv2_atom_sequence_append_event(self->midi_port_out,outCapacity, &out.event);
			}
		}

		if (++ pattern->elapsed == patternEnd)
			pattern->elapsed = 0;

//NOTE it appears one pitch can't be played twice on a given channel... TODO check
	}
//XXX may need to check the outCapacity is not exceeded	
}

/*
   The plugin must store the data location, but data may not be accessed except in run().
   This method is in the 'audio' threading class, and is called in the same context as run().
*/
static void connect_port(LV2_Handle instance, uint32_t port, void* data)
{
//XXX looks like its meant to called over and over

	Self* self = (Self*)instance;

	switch (port) {
		case CONTROL_IN:
			self->control_port_in = (LV2_Atom_Sequence*)data;
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
	self->state = NOTE_OFF;
}

/*
   Update the current position based on a host message.  This is called by
   run() when a time:Position is received.
*/
static void updatePosition(Self* self, const LV2_Atom_Object* obj)
{
//XXX every few seconds seems to send through another 'speed' message	
printf("updatePosition -1 \n");
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
	if (speed && speed->type == uris->atom_Float)  {
		self->speed = ((LV2_Atom_Float*)speed)->body;

printf("updatePosition CHANGED  self->speed: %lf\n",self->speed);
}

    // Received a beat position, synchronise
    // This hard sync may cause clicks, a real plugin would be more graceful

	if (beat && beat->type == uris->atom_Float) {
		const float frames_per_beat = (float)(60.0 / self->bpm * self->sampleRate);
		const float bar_beats       = ((LV2_Atom_Float*)beat)->body;
		const float beat_beats      = bar_beats - floorf(bar_beats);
		self->elapsedLen           = (uint32_t)(beat_beats * frames_per_beat);

//XXX how does this relate the the play() state changes?		
		if (self->elapsedLen < self->noteLen)
			self->state = NOTE_ON;
		else 
			self->state = NOTE_OFF;
	}
}

/* `run()` must be real-time safe. No memory allocations or blocking! */
static void run(LV2_Handle instance, uint32_t sample_count)
{
	Self* self = (Self*)instance;
 	const URIs* uris = &self->uris;

  	/* Initially self->out_port contains a Chunk with size set to capacity */
	const uint32_t outCapacity = self->midi_port_out->atom.size;

	/* Write an empty Sequence header to the output */
	lv2_atom_sequence_clear(self->midi_port_out);
	self->midi_port_out->atom.type = self->uris.atom_Sequence;

	/* Loop through events: */
	const LV2_Atom_Sequence* in = self->control_port_in;
	uint32_t last_t = 0;

	LV2_ATOM_SEQUENCE_FOREACH (self->control_port_in, ev) {

//XXX Q: should we calling this 'play' for all message types? 
		// Play the click for the time slice from last_t until now
		play(self, last_t, (uint32_t)ev->time.frames,outCapacity);

		/* Handle a position message */
		if (ev->body.type == uris->atom_Object || /*deprecated*/ ev->body.type == uris->atom_Blank) {
			const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&ev->body;

			if (obj->body.otype == uris->time_Position) 
				updatePosition(self, obj);
		}

		last_t = (uint32_t)ev->time.frames;
	}

	/* Play out the remainder of cycle: */
	play(self, last_t, sample_count, outCapacity);
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

