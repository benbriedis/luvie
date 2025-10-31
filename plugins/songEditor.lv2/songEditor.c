
#include <assert.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/core/lv2_util.h>
#include <lv2/log/logger.h>
#include <lv2/midi/midi.h>
#include <lv2/patch/patch.h>
#include <lv2/time/time.h>
#include <lv2/urid/urid.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PLUGIN_URI "http://benbriedis.com/lv2/luvie"

typedef enum {
	NOTE_ON,
	NOTE_OFF,
} State;

typedef struct {
	float start;
	float pitch;
	float lengthInBeats;
	float velocity;

	int state; //XXX probably wont need long term
} Note;

typedef struct {
    char name[20];
    int lengthInBeats;
    int baseOctave;
    int baseNote;

//TODO add state: off/limited/unlimited
//TODO add startInFrames
//TODO add lengthInFrames

	//XXX patterns can start in different places from each, and be of different lengths
	uint32_t positionInFrames; 

//TODO generalise the number of notes
	Note notes[5];
	//TODO probably add a MIDI channel here
} Pattern;

//XXX cf guaranteeing that the notes appear in order. Then can maintain a 'next note' for each pattern (maybe not worthwhile though...)
Pattern patterns[] = {{
	"pattern1", 4, 3, 0, 0, {
		{0.0, 0, 0.5, 100, NOTE_OFF},
		{0.0, 7, 0.5, 100, NOTE_OFF},
		{1.0, 5, 0.5, 100, NOTE_OFF},
		{2.0, 7, 0.5, 100, NOTE_OFF},
		{3.0, 9, 0.5, 100, NOTE_OFF},
	}
}, {
	"pattern2", 4, 3, 0,  0, {
		{0.0, 8, 0.5, 100, NOTE_OFF},
		{0.0, 7, 0.5, 100, NOTE_OFF},
		{1.0, 5, 0.5, 100, NOTE_OFF},
		{2.0, 7, 0.5, 100, NOTE_OFF},
		{3.0, 7, 0.5, 100, NOTE_OFF},
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

LV2_URID time_position;
LV2_URID time_beat;
LV2_URID time_bar;
	
	LV2_URID time_Rate;
	LV2_URID time_Time;
	LV2_URID time_beatUnit;
	LV2_URID time_frame;
	LV2_URID time_framesPerSecond;
	LV2_URID time_beatsPerBar;

	LV2_URID midi_Event;
	LV2_URID patch_Set;
	LV2_URID patch_property;
	LV2_URID patch_value;
} URIs;


enum { CONTROL_IN = 0, MIDI_OUT = 1 };

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
//XXX THINK this might exist for jogging etc	
	float speed; // Transport speed (usually 0=stop, 1=play)

	Pattern* patterns;
	int numPatterns;
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
printf("CALLING instantiate()\n");			

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

	uris->time_position			= map->map(map->handle, LV2_TIME__position);
	uris->time_beat				= map->map(map->handle, LV2_TIME__beat);
	uris->time_bar				= map->map(map->handle, LV2_TIME__bar);

	uris->time_Rate				= map->map(map->handle, LV2_TIME__Rate);
	uris->time_Time				= map->map(map->handle, LV2_TIME__Time);
	uris->time_beatUnit			= map->map(map->handle, LV2_TIME__beatUnit);
	uris->time_frame			= map->map(map->handle, LV2_TIME__frame);
	uris->time_framesPerSecond	= map->map(map->handle, LV2_TIME__framesPerSecond);
	uris->time_beatsPerBar		= map->map(map->handle, LV2_TIME__beatsPerBar);

	/* Initialise instance data: */
	self->sampleRate = sampleRate;
	self->bpm = 120.0f;

	self->patterns = calloc(1,sizeof(patterns));
	if (!self)
		return NULL;
	memcpy(self->patterns,patterns,sizeof(patterns));

	return (LV2_Handle)self;
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
printf("CALLING activate()\n");			

	Self* self = (Self*)instance;

	self->numPatterns = 2;

	for (int p=0; p<self->numPatterns; p++) {     //FIXME
		Pattern* pattern = &self->patterns[p];
//XXX doesnt have to start at 0.		
		pattern->positionInFrames = 0;

		for (int n=0; n<5; n++) {  //FIXME
			Note* note = &pattern->notes[n];
			note->state = NOTE_OFF;
		}
	}
}

//static void gotMessage()
//{
	/* Updates:
		[
			{patternNum:1, state:limited/unlimited/off, position:inFrames, length:inFrames},  TODO cf BPM changes. Would beats be easier?
			{patternNum:1, state:limited/unlimited/off, position:inFrames, length:inFrames}
		]

		Also cf add pattern, delete pattern
	*/

//}

//static void sendMessage()
//{
	//XXX if a note has been added or removed whil playing, tell the host
//}


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

//TODO THINK how might speed affect things here?
	if (self->speed == 0.0f) 
		return;

//TODO can we use logger? 

//TODO presumbly could cut this up into segments for a more efficient solution.
//     Could/should create an index that contains all note start and end offsets in order.
//     Store a 'last position' and go from there each time. The states can be stored with the patterns.

	MIDINoteEvent out;

	for (uint32_t i = begin; i < end; ++i) {

		for (int p=0; p < self->numPatterns; p++) {

		//TODO maybe... could add a fastForward(). Search through all notes for next event and add to i

			Pattern* pattern = &self->patterns[p];

			int patternEnd = pattern->lengthInBeats * framesPerBeat; 

			for (int j=0; j<5; j++) {  //FIXME
				Note* note = &pattern->notes[j]; 

				int noteStart = note->start * framesPerBeat;
				int noteLen = note->lengthInBeats * framesPerBeat;
				int noteEnd = noteStart + noteLen;

				int pitch = pattern->baseOctave * 12 + pattern->baseNote + note->pitch;

				if (note->state == NOTE_OFF && pattern->positionInFrames >= noteStart && pattern->positionInFrames < noteEnd) {
//TODO separate out the contents					
					note->state = NOTE_ON;

					out.event.time.frames = i;
					out.event.body.type =  self->uris.midi_Event;
					out.event.body.size = 3;
					out.msg[0] = 0x90;		// Note On for channel 0
					out.msg[1] = pitch;
					out.msg[2] = 100;		// Velocity

					lv2_atom_sequence_append_event(self->midi_port_out, outCapacity, &out.event);
				}

				else if (note->state == NOTE_ON && pattern->positionInFrames == noteEnd - 1) {
					note->state = NOTE_OFF;

					out.event.time.frames = i;
					out.event.body.type = self->uris.midi_Event;
					out.event.body.size = 3;
					out.msg[0] = 0x80; 	// Note off
					out.msg[1] = pitch;       
					out.msg[2] = 0;       //XXX this is how fast to release note (127 fastest). Not always (often?) implemented?

					lv2_atom_sequence_append_event(self->midi_port_out,outCapacity, &out.event);
				}
			}

			if (++ pattern->positionInFrames == patternEnd)
				pattern->positionInFrames = 0;
		}

//NOTE it appears one pitch can't be played twice on a given channel... TODO check
	}
//XXX may need to check the outCapacity is not exceeded	
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

LV2_Atom* Position = NULL;
LV2_Atom* position = NULL;
LV2_Atom* time_beat = NULL;
LV2_Atom* time_bar = NULL;

LV2_Atom* time_Rate = NULL;
LV2_Atom* time_Time = NULL;
LV2_Atom* time_beatUnit = NULL;
LV2_Atom* time_frame = NULL;
LV2_Atom* time_framesPerSecond = NULL;
LV2_Atom* time_beatsPerBar = NULL;

	lv2_atom_object_get(obj,
		uris->time_barBeat, &beat,
		uris->time_beatsPerMinute, &bpm,
		uris->time_speed, &speed,

uris->time_Position, &Position,
uris->time_position, &position,
uris->time_beat, &time_beat,
uris->time_bar, &time_bar,

uris->time_Rate, &time_Rate,
uris->time_Time, &time_Time,
uris->time_beatUnit, &time_beatUnit,
uris->time_frame, &time_frame,
uris->time_framesPerSecond, &time_framesPerSecond,
uris->time_beatsPerBar, &time_beatsPerBar,
		NULL
	);

printf("Position: %ld\n",(long)Position);  //XXX NOT SENT

printf("Rate: %ld\n",(long)time_Rate);

printf("Time: %ld\n",(long)time_Time);


printf("position: %ld\n",(long)position);  //XXX NOT SENT

if (time_beat) 
printf("time_beat: %lf\n",((LV2_Atom_Double*)time_beat)->body);  //XXX NOT SENT
else
printf("time_beat: %ld\n",(long)time_beat);

if (time_bar) 
printf("time_bar: %ld\n",((LV2_Atom_Long*)time_bar)->body);

if (speed) 
printf("speed: %f\n",((LV2_Atom_Float*)speed)->body);

if (beat) 
printf("barBeat: %f\n",((LV2_Atom_Float*)beat)->body);

if (bpm) 
printf("bpm: %f\n",((LV2_Atom_Float*)bpm)->body);

if (time_beatUnit)
printf("beatUnit: %d\n",((LV2_Atom_Int*)time_beatUnit)->body);

if (time_frame)
printf("frame: %ld\n",((LV2_Atom_Long*)time_frame)->body);
//TODO check this stays in sync with ev->time

printf("framesPerSecond: %ld\n",(long)time_framesPerSecond);

if (time_beatsPerBar)
printf("beatsPerBar: %f\n",((LV2_Atom_Float*)time_beatsPerBar)->body);

printf("\n");

    /* Tempo changed */
	if (bpm && bpm->type == uris->atom_Float)		//XXX why the 1st guard?
		self->bpm = ((LV2_Atom_Float*)bpm)->body;

	/* Speed changed, e.g. 0 (stop) to 1 (play) */
	if (speed && speed->type == uris->atom_Float)  		//XXX why the 1st guard?
		self->speed = ((LV2_Atom_Float*)speed)->body;
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

		/* Handle a position message */    //NOTE the metronome had this after the play() call. Dont know why.
		if (ev->body.type == uris->atom_Object || ev->body.type == uris->atom_Blank) {
			const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&ev->body;

			if (obj->body.otype == uris->time_Position) 
				updatePosition(self, obj);
		}


//XXX Q: should we calling this 'play' for all message types? 
		// Play the click for the time slice from last_t until now
		play(self, last_t, (uint32_t)ev->time.frames,outCapacity);

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
{
printf("CALLING deactivate\n");			
}

/*
   Destroy a plugin instance (counterpart to `instantiate()`).

   This method is in the ``instantiation'' threading class, so no other
   methods on this instance will be called concurrently with it.
*/
static void cleanup(LV2_Handle instance)
{
	Self* self = (Self*)instance;

	free(self->patterns);
	free(self);
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


//TODO later test that the output is all in time

