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

#define PLUGIN_URI "https://github.com/benbriedis/luvie/harmony"

typedef enum {
	NOTE_ON,
	NOTE_OFF,
} State;

typedef enum {
	PATTERN_OFF,
	PATTERN_ON
} PatternState;

typedef struct {
	float start;
	float pitch;
	float lengthInBeats;
	float velocity;
	int state; 
} Note;

//TODO add a pattern ID. cf name ==> label

typedef struct {
    char name[20];
    int lengthInBeats;
    int baseOctave;
    int baseNote;
	int state;
//TODO possibly/probably replace these two with endInFrames;	
	uint32_t startInFrames; 
	uint32_t lengthInFrames; 
	uint32_t positionInFrames; 
//TODO generalise the number of notes
	Note notes[5];
//TODO probably add a MIDI channel here
} Pattern;

//XXX cf guaranteeing that the notes appear in order. Then can maintain a 'next note' for each pattern (maybe not worthwhile though...)
Pattern patterns[] = {{
	"pattern1", 4, 3, 0, 0, PATTERN_OFF, 0, 0,  {
		{0.0, 0, 0.5, 100, NOTE_OFF},
		{0.0, 7, 0.5, 100, NOTE_OFF},
		{1.0, 5, 0.5, 100, NOTE_OFF},
		{2.0, 7, 0.5, 100, NOTE_OFF},
		{3.0, 9, 0.5, 100, NOTE_OFF},
	}
}, {
	"pattern2", 4, 3, 0, 0, PATTERN_OFF, 0, 0, {
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
			...
		]
	},
	...
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
	LV2_URID time_beatsPerMinute;
	LV2_URID time_speed;
	LV2_URID midi_Event;
	LV2_URID patch_Set;
	LV2_URID patch_property;
	LV2_URID patch_value;

	LV2_URID loopsMessage;
	LV2_URID loopId;
	LV2_URID loopEnable;
	LV2_URID loopStartFrame;
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
	const LV2_Atom_Sequence* controlPortIn;
	LV2_Atom_Sequence* midi_port_out;

	URIs uris;   // Cache of mapped URIDs

	// Variables to keep track of the tempo information sent by the host
	double sampleRate; 
	float bpm;
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
printf("CALLING instantiate() - 1\n");			

	Self* self = (Self*)calloc(1, sizeof(Self));
	if (!self)
		return NULL;

printf("CALLING instantiate() - 2\n");			
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
printf("CALLING instantiate() - 4\n");			

	// Map URIS
	URIs* const uris = &self->uris;
	LV2_URID_Map* const map = self->map;

//XXX do I still need all of these?
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
	uris->time_beatsPerMinute = map->map(map->handle, LV2_TIME__beatsPerMinute);
	uris->time_speed          = map->map(map->handle, LV2_TIME__speed);

	uris->loopsMessage        = map->map(map->handle, "https://github.com/benbriedis/luvie#loopsMessage");
	uris->loopId              = map->map(map->handle, "https://github.com/benbriedis/luvie#loopId");
	uris->loopEnable          = map->map(map->handle, "https://github.com/benbriedis/luvie#loopEnable");
	uris->loopStartFrame      = map->map(map->handle, "https://github.com/benbriedis/luvie#loopStartFrame");

	/* Initialise instance data: */
	self->sampleRate = sampleRate;
	self->bpm = 120.0f;

printf("CALLING instantiate() - 5\n");			
	self->patterns = calloc(1,sizeof(patterns));
	if (!self)
		return NULL;
	memcpy(self->patterns,patterns,sizeof(patterns));

printf("CALLING instantiate() - 6\n");			
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
			self->controlPortIn = (LV2_Atom_Sequence*)data;
			break;

		case MIDI_OUT:
			self->midi_port_out = (LV2_Atom_Sequence*)data;
			break;
	}
}

/* The plugin must reset all internal state */
static void activate(LV2_Handle instance)
{
printf("CALLING harmony.c activate() - start\n");			

	Self* self = (Self*)instance;

	self->numPatterns = 2;

	for (int p=0; p<self->numPatterns; p++) {     //FIXME
		Pattern* pattern = &self->patterns[p];
//XXX doesnt have to start at 0.		
		pattern->positionInFrames = 0;

		pattern->state = PATTERN_ON;
		pattern->startInFrames = 0;
		pattern->lengthInFrames = 0;

		for (int n=0; n<5; n++) {  //FIXME
			Note* note = &pattern->notes[n];
			note->state = NOTE_OFF;
		}
	}
printf("CALLING harmony.c activate() - end\n");			
}

//static void gotMessage()
//{
	/* Updates:
		[
			{id:4, state:on/off, offset:inFrames},  TODO cf BPM changes. Would beats be easier?
			{id:6, state:on/off, offset:inFrames}
		]

	  Atom version:

			Event (but maybe Atom):        (event has timestamp too. Can/should we separate the atoms - probably not... 
											always triggered by a time change. Except when the user has done something.)
			   Vector:                     (pattern instructions)
				  Object
					 id: Int               (cf URID or URI)
					 state: Int            (cf literal)
					 offset: Long        (using Time.Position.frame would be possible, but over the top)
         
		Also cf add pattern, delete pattern

		ALSO cf supporting a basic 'default' mode or other hosts:
		     play all patterns from the start (no offsets) in loops.

		NOTE I've considered making state:ON/UNLIMITED/LIMITED where LIMITED has an length that can include multiple loops.
		     Ultimately doesn't seem to be worthwhile. Any other host that used such a feature would need to be written to support it -
			 and in these cases explicitly turning off a looping pattern is easy enough. Note that hosts need to be able to support
			 muting which is pretty dynamic. Also it would complicate the implementation of future plugins.
	*/

//}

//static void sendMessage()
//{
	//XXX if a note has been added or removed whil playing, tell the host
//}

//TODO can we use logger? 

typedef struct {
	LV2_Atom_Event event;
	uint8_t msg[3];
} MIDINoteEvent;

static void noteOn(Self* self,Note* note,uint32_t position,int pitch,uint32_t outCapacity)
{
	MIDINoteEvent out;

	note->state = NOTE_ON;

	out.event.time.frames = position;
	out.event.body.type =  self->uris.midi_Event;
	out.event.body.size = 3;
	out.msg[0] = 0x90;		// Note On for channel 0
	out.msg[1] = pitch;
	out.msg[2] = 100;		// Velocity

//XXX may need to check the outCapacity is not exceeded	
	lv2_atom_sequence_append_event(self->midi_port_out, outCapacity, &out.event);
}

static void noteOff(Self* self,Note* note,uint32_t position,int pitch,uint32_t outCapacity)
{
	MIDINoteEvent out;

	note->state = NOTE_OFF;

	out.event.time.frames = position;
	out.event.body.type = self->uris.midi_Event;
	out.event.body.size = 3;
	out.msg[0] = 0x80; 	// Note off
	out.msg[1] = pitch;       
	out.msg[2] = 0;       //XXX this is how fast to release note (127 fastest). Not always (often?) implemented?

	lv2_atom_sequence_append_event(self->midi_port_out,outCapacity, &out.event);
}

//NOTE it appears one pitch can't be played twice on a given channel... TODO check


/* Play MIDI events in the range [begin..end) relative to this cycle.  */
static void playPattern(Self* self, Pattern* pattern, uint32_t begin, uint32_t end, uint32_t outCapacity)
{
	const uint32_t framesPerBeat = (uint32_t)(60.0f / self->bpm * self->sampleRate);

	int patternEnd = pattern->lengthInBeats * framesPerBeat; 

	//TODO could probably make more efficient... cf moving the 'j' loop to around to 'i' loop. Calculate the notes on 
	//     and off, then sort and output.

	for (uint32_t i = begin; i < end; i++) {
		for (int j=0; j<5; j++) {  //FIXME
			Note* note = &pattern->notes[j]; 

			int noteStart = note->start * framesPerBeat;
			int noteLen = note->lengthInBeats * framesPerBeat;
			int noteEnd = noteStart + noteLen;

			int pitch = pattern->baseOctave * 12 + pattern->baseNote + note->pitch;

			if (note->state == NOTE_OFF && pattern->positionInFrames >= noteStart && pattern->positionInFrames < noteEnd) 
				noteOn(self,note,i,pitch,outCapacity);

			else if (note->state == NOTE_ON && pattern->positionInFrames == noteEnd - 1) 
				noteOff(self,note,i,pitch,outCapacity);
		}

		if (++ pattern->positionInFrames == patternEnd)
			pattern->positionInFrames = 0;
	}

//TODO if we reach the end of a once-through pattern disable it.
}

static void playPatterns(Self* self, uint32_t begin, uint32_t end, uint32_t outCapacity)
{
	for (int p=0; p < self->numPatterns; p++) {
		Pattern* pattern = &self->patterns[p];

		if (pattern->state != PATTERN_OFF)
			playPattern(self,pattern,begin,end,outCapacity);
	}
}

/*
   Update the current position based on a host message.  This is called by
   run() when a time:Position is received.
*/
static void updatePosition(Self* self, const LV2_Atom_Object* timeObj)
{
	const URIs* uris = &self->uris;

	LV2_Atom* bpm = NULL;
	LV2_Atom* speed = NULL;

	lv2_atom_object_get(timeObj,
		uris->time_beatsPerMinute, &bpm,
		uris->time_speed, &speed,
		NULL
	);

    /* Tempo changed */
	if (bpm && bpm->type == uris->atom_Float)
		self->bpm = ((LV2_Atom_Float*)bpm)->body;

	/* Speed=0 (stop) or speed=1 (play) */
	if (speed && speed->type == uris->atom_Float)
		self->speed = ((LV2_Atom_Float*)speed)->body;
}

/* `run()` must be real-time safe. No memory allocations or blocking! */
static void run(LV2_Handle instance, uint32_t sample_count)
{
//printf("CALLING harmony.c run() - start\n");			

	Self* self = (Self*)instance;
 	const URIs* uris = &self->uris;

  	/* Initially self->out_port contains a Chunk with size set to capacity */
	const uint32_t outCapacity = self->midi_port_out->atom.size;

	/* Write an empty Sequence header to the output */
	lv2_atom_sequence_clear(self->midi_port_out);
	self->midi_port_out->atom.type = self->uris.atom_Sequence;

//TODO remove time position messages	

	/* Loop through events: */
	const LV2_Atom_Sequence* in = self->controlPortIn;
	uint32_t last_t = 0;

	LV2_ATOM_SEQUENCE_FOREACH (self->controlPortIn, ev) {

		//XXX empty sequences seem to send through single empty events
		if (ev->body.type != 0)
			printf("GOT EVENT type: %d   size: %d\n",ev->body.type,ev->body.size);

//XXX Q: should we calling this 'play' for all message types? 
		// Play the click for the time slice from last_t until now
		if (self->speed != 0.0f) 
			playPatterns(self, last_t, (uint32_t)ev->time.frames,outCapacity);

		/* 
			NOTE position messages sometimes come part way through a cycle (eg 1024 frames).
			In this case the Position frames includes the start of the standard cycle.
		*/

//		if (lv2_atom_forge_is_object_type(const LV2_Atom_Forge *forge, uint32_t type))  XXX Use this in future if we create a forge
		if (ev->body.type == uris->atom_Object || ev->body.type == uris->atom_Blank) {
			const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&ev->body;

printf("GOT an atom  otype: %d\n",obj->body.otype);

			if (obj->body.otype == uris->time_Position) 
				updatePosition(self, obj); //TODO delete I think
			else if (obj->body.otype == uris->loopsMessage) {
printf("GOT loops message\n");

				LV2_Atom* loopId = NULL;
				LV2_Atom* loopEnable = NULL;
				LV2_Atom* startFrame = NULL;

				lv2_atom_object_get(obj,
					uris->loopId, &loopId,
					uris->loopEnable, &loopEnable,
					uris->loopStartFrame, &startFrame, //TODO its optional. How to deal with?
					NULL
				);

//NOTE I'm favouring use of "pattern" again over "loop"				
				printf("loopId: %ld\n",((LV2_Atom_Long*)loopId)->body);
				printf("loopEnable: %b\n",((LV2_Atom_Bool*)loopEnable)->body);
				if (startFrame)
					printf("startFrame: %ld\n",((LV2_Atom_Long*)startFrame)->body);

/*
				if (loopId && loopId->type == uris->atom_Long)  //XXX they should really be an exceptions
					self->loopId = ((LV2_Atom_Long*)loopId)->body;

				if (loopEnable /*&& speed->type == uris->atom_Bool* /)
					self->loopEnable = ((LV2_Atom_Bool*)loopEnable)->body;

				if (startFrame /*&& speed->type == uris->atom_Long* /)
					self->loopStartFrame = ((LV2_Atom_Long*)startFrame)->body;
*/				

			}

//TODO probably accept another atom type message to change pattern indexes (and maybe allow patterns to be added or removed).
//     Maybe a CC or CV message. MIDI is maybe a possibility. Otherwise custom.
		}


		last_t = (uint32_t)ev->time.frames;
	}

	/* Play out the remainder of cycle: */
	if (self->speed != 0.0f) 
		playPatterns(self, last_t, sample_count, outCapacity);
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

