#include "lv2/atom/forge.h"
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#define PLUGIN_URI "http://benbriedis.com/lv2/songEditor"

typedef enum {
	NOTE_ON,
	NOTE_OFF,
} State;

typedef enum {
	PATTERN_OFF,
	PATTERN_LIMITED,
	PATTERN_UNLIMITED
} PatternState;

typedef enum {
	TRACK_PLAYING,
	TRACK_NOT_PLAYING
} TrackState;


typedef struct {
	float start;
	float pitch;
	float lengthInBeats;
	float velocity;
	int state; 
} Note;


typedef struct {
	int bar;
	int top;		//TODO better names for top and bottom?
	int bottom;
} TimeSignature;

typedef struct {
	int bar;
	float beat;
} Position;

typedef struct {
	Position position;  //TODO allow start and end instead. Need to use a union
	float value;
} Bpm;

typedef struct {
	int bar;	/* Indexed from 0 */
	float beat; /* Indexed from 0 */
} Interval;

typedef struct {
	int id;
	Interval start;
	float length;
	/* Calculated values: */
	long startInFrames;
	long endInFrames;
} Pattern;

typedef struct {
	int id;
	char label[8];
	int numPatterns;
	Pattern patterns[2];
	int state;
	int currentOrNext;
} Track;

typedef struct {
	TimeSignature timeSignatures[1];
	Bpm bpms[1];
	Track tracks[1];
} Timeline;

//TODO use these time signatures and bpms
Timeline timeline = {
	{{0,0,0}},
	{{{140}}},
	{
		{7,"Track 8", 2, {{
			5, {2, 0.0}, 4.0, 0, 0
		},{
			6, {5, 1.0}, 3.0, 0, 0
		}}, TRACK_NOT_PLAYING, 0 }
	}
};



/*XXX JSON-style:

XXX cf variable time signatures. NEED A SEPARATE TIMELINE.  PROBABLY WORTH SUPPORTING...

const timeline = {
	timeSignatures: [{
		bar: 7,
		top: 3 		TODO better names for these?
		bottom: 4
	}],
	bpms: [{
			start: {bar: 6,beat 3.0}, end:{bar: 8,beat 3.0},    # A linear ramp
			value: 140
		}, {
			position: {bar: 6,beat 3},
			value: 140
		},
	],
	tracks: [
	]
};

const tracks = [
	{
		id: 7,
		label: 'Track 1',
		patterns: [{
			id:5,
			start: {
				bar: 7,     XXX cf allowing beats only, no bars?  bar could be omitted. PROBABLY no real value in this option.
				beat: 0.0,   XXX allow fractions
				offset: 0.0
			}, 
			length: 8.0   (# beats)
		}]
	}
]
*/

/*
cf Precalculate frames of each bar? Recalc if user manually changes something.
   OR just when there is a jump in the transport... we'd need to compare the frame with the expected frame or something.
      Calculate on a needs basis... probably the best.


   Probably send frames to the plugins, offset against the start of each pattern individually.
*/

//TODO read up on CC and CV, OSC



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
	LV2_URID time_speed;
	LV2_URID time_frame;

//TODO probably delete. Replace some other Atom...	
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
	float speed; // Transport speed (usually 0=stop, 1=play)

	float bpm; //XXX NOT SURE we want this here

	Timeline* timeline;

	long numTracks;

	long positionInFrames;

	/* Used to check transport jumps seem to be working OK */
	float lastSpeed;
	long testPositionInFrames;
} Self;

static void initPatternEndPoints(Self* self)
{
	const uint32_t framesPerBeat = (uint32_t)(60.0f / self->bpm * self->sampleRate);

//TODO handle multiple BPM and time signatures
	int beatsPerBar = 4;
	
	for (int t=0; t<self->numTracks; t++) {
		Track* track = &self->timeline->tracks[t];

		for (int p=0; p<track->numPatterns; p++) {
			Pattern* pattern = &track->patterns[p];

			//XXX assuming start.beat starts at 0.0
			float startBeat = pattern->start.bar*beatsPerBar + pattern->start.beat;
			float endBeat = startBeat + pattern->length;

			pattern->startInFrames = startBeat * framesPerBeat;
			pattern->endInFrames = endBeat * framesPerBeat;

printf("startBeat: %f  startInFrames: %ld\n",startBeat,pattern->startInFrames);			
printf("  endBeat: %f    endInFrames: %ld\n",endBeat,pattern->endInFrames);			
		}
	}
}

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
	uris->time_speed          = map->map(map->handle, LV2_TIME__speed);
	uris->time_frame			= map->map(map->handle, LV2_TIME__frame);

	/* Initialise instance data: */
	self->sampleRate = sampleRate;
	self->bpm = 120.0f;
	self->positionInFrames = 0;
	self->testPositionInFrames = 0;

	self->timeline = calloc(1,sizeof(timeline));
	if (!self)
		return NULL;
	memcpy(self->timeline,&timeline,sizeof(timeline));

	self->numTracks = 1;

	initPatternEndPoints(self);

	for (int t=0; t<self->numTracks; t++) 
		self->timeline->tracks[t].state = TRACK_NOT_PLAYING;

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

//TODO ==> CONTROL_OUT
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

	/* NOTE positionInFrames shouldn't be set here */

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

//TODO can we use logger? 


//	lv2_atom_sequence_append_event(self->midi_port_out, outCapacity, &out.event);


static void playTrack(Self* self, Track* track, long begin, long end, uint32_t outCapacity)
{
//TODO reinit currentPattern/nextPattern on jumps

	long pos = begin;

//printf("playTrack pos: %d   endPos: %ld\n",end,endPos);

	/* In theory a cycle can contain many note changes. Best to support it, however unlikely and undesirable. */
	for (int pat=track->currentOrNext; pos<end && pat<track->numPatterns; pat++) {

		Pattern* pattern = &track->patterns[track->currentOrNext];

		/* NOTE a track can only play one pattern at a time */

//FIXME current luvie.c is receiving the pattern length and turning itself off. Probably better not to.		

		if (track->state == TRACK_PLAYING && pos >= pattern->endInFrames) {
			//turnPatternOff();
			printf("TURNING OFF PATTERN %d\n",track->currentOrNext);

			track->state = TRACK_NOT_PLAYING;
			pos = pattern->endInFrames;

			track->currentOrNext++;
			if (track->currentOrNext == track->numPatterns)
				break;
			pattern = &track->patterns[track->currentOrNext];
		}

//TODO could there be a problem if a pattern starts at (0,0)?

		/* NOTE a single loop can turn one pattern off and another on */

		if (track->state == TRACK_NOT_PLAYING && pos >= pattern->startInFrames) {
			//turnPatternOn();
			printf("TURNING ON PATTERN %d\n",track->currentOrNext);

			track->state = TRACK_PLAYING;
			pos = pattern->startInFrames;
		}
	}
}

/* Play patterns in the range [begin..end) relative to this cycle.  */
static void playSong(Self* self, long begin, long end, uint32_t outCapacity)
{
//printf("playSong()  positionInFrames: %ld\n",self->positionInFrames);
//TODO add mute and solo features for tracks

	for (int t=0; t < self->numTracks; t++) {
		Track* track = &self->timeline->tracks[t];

		playTrack(self,track,begin,end,outCapacity);
	}
}

/*
	PRINCIPLES...

	We can operate in either of 2 modes - live looper and song mode.
	When in song mode we have a fixed time signature, so we can't really use the host's beatsPerBar or beatUnit. SO IGNORE THESE.

	If synching with Ardour or similar we probably want to use the frames so we can sync with Ardour audio tracks. SO USE THE FRAMES.
	Using the bar/barBeat pair might be possible, but would be confusing. 
	KIND-OF possible, but I think when it comes to song editors sequencers a single source of truth is desirable. 
	SO OMIT FOR THE MOMENT. Syncing with other sequencers is an advanced feature, and probably nasty to use anyway in practice.

	We could use the host's BPM's if we really wanted, but the SOS argument suggests we shouldn't. SO IGNORE THIS.
*/
static void maybeJump(Self* self, const LV2_Atom_Object* obj, uint32_t offsetFrames)
{
	const URIs* uris = &self->uris;

	/*
		Ardour seems to take a while to establish itself on play. Possibly addressing latency issues.
		Makes it hard to know when to use it when to jump.

//TODO PLAN:
// Try using 'speed' changes to determine when to use time_frame to determine when to jump. Otherwise ignore position changes.
// Otherwise research...
	*/

	LV2_Atom* speed = NULL;
	LV2_Atom* time_frame = NULL;

	lv2_atom_object_get(obj,
		uris->time_speed, &speed,
		uris->time_frame, &time_frame,
		NULL
	);

//TODO if speed=0 probably turn off all states	

	/* I believe newPos incorporates the offsetFrames */
	long newPos = ((LV2_Atom_Long*)time_frame)->body;
	float newSpeed = ((LV2_Atom_Float*)speed)->body;

if (speed) 
printf("speed: %f\n",newSpeed);

if (time_frame) 
printf("frame: %ld\n",newPos);

printf("positionInFrames: %ld\n",self->positionInFrames);
printf("offsetFrames: %d\n",offsetFrames); 



	if (self->speed==1.0f && newSpeed==0.0f) {
		self->testPositionInFrames = newPos;

		for (int t=0; t<self->numTracks; t++) {
			self->timeline->tracks[t].state = TRACK_NOT_PLAYING;
			//TODO send stop message to child plugins
		}
	}
	else if (self->speed==0.0f && newSpeed==1.0f) {
		if (newPos != self->testPositionInFrames + offsetFrames) {
//XXX separate into function			
			printf("TRANSPORT JUMPED\n");

			/* Set currentOrNext to match the new position */

			for (int t=0; t<self->numTracks; t++) {
				Track* track = &self->timeline->tracks[t];
				track->currentOrNext = track->numPatterns;

				for (int p=0; p<track->numPatterns; p++) {
					Pattern* pattern = &track->patterns[p];

					if (pattern->startInFrames >= newPos) {
						track->currentOrNext = p;
					 	break;
					}
				}
			}
		}
	}
	else if (self->speed==1.0f && newSpeed==1.0f) {
		if (newPos != self->positionInFrames + offsetFrames)
			printf("TRANSPORT JUMPED WHILE TRANSPORT RUNNING\n");
//TODO adjust pattern states? Maybe just let run and log a warning
	}

//NOTE stopping and start in Ardour appears to jump back 3 "cycles"	(ie 3 x 1024frames).
//     One implication is that everything the transport jumps/restarts we should recalculate our state.
//     TODO maybe better not to have this state. Just delete it and assume when a boundary is crossed a stop or start message is sent.
//     OR ELSE check the state is correct after every jump and amend.
//     IF we jump into a changed position with different state we really need to old state so we can send the correct messages.

	/* 
		Ardour serves up some negative values while speed=0 and the transport is at the start.
		The frames seem to be aligned OK when speed=1.
	*/
	self->positionInFrames = newPos;


printf("\n");

//FIXME the speed (and frame) guard needs to be earlier...	

	/* Speed=0 (stop) or speed=1 (play) */
	if (speed && speed->type == uris->atom_Float) 
		self->speed = newSpeed;
}

//XXX another use case muting or soloing a track should probably instantly start or stop the current pattern

/* `run()` must be real-time safe. No memory allocations or blocking! */
static void run(LV2_Handle instance, uint32_t sampleCount)
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
	uint32_t timeOffset = 0;

int i=0;	
	LV2_ATOM_SEQUENCE_FOREACH (self->control_port_in, ev) {
//XXX cf 'handlePartCycle()'

//FIXME move the Position code above the play() code in luvie.c too 		

		// Play the click for the time slice from last_t until now
		if (self->speed != 0.0f) 
			playSong(self, self->positionInFrames+timeOffset, self->positionInFrames+ev->time.frames,outCapacity);

		/* ev->time.frames is usually 0 - except for when a position message comes part way through a cycle. */

printf("time->frames: %ld\n",ev->time.frames);	//ARDOUR USUALLY RETURNS 0 HERE, 
//printf("time->beats: %lf\n",ev->time.beats);				

		/* 
			NOTE position messages sometimes come part way through a cycle (eg 1024 frames).
			In this case the Position frames includes the start of the standard cycle.
		*/

//		if (lv2_atom_forge_is_object_type(const LV2_Atom_Forge *forge, uint32_t type))  XXX Use this in future if we create a forge
		if (ev->body.type == uris->atom_Object || ev->body.type == uris->atom_Blank) {
			const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&ev->body;

			if (obj->body.otype == uris->time_Position)  {
printf("i: %d  sampleCount:%d\n",i,sampleCount);				
				maybeJump(self, obj,(uint32_t)ev->time.frames);
			}
			else
printf("GOT A DIFFERENT TYPE OF MESSAGE  otype: %d\n",obj->body.otype);

//TODO probably accept another atom type message to change pattern indexes (and maybe allow patterns to be added or removed).
//     Maybe a CC or CV message. MIDI is maybe a possibility. Otherwise custom.
		}

if (i!=0)
printf("i: %d\n",i);				

//XXX Q: should we calling this 'play' for all message types? 

		timeOffset = (uint32_t)ev->time.frames;
i++;		
	}

	/* Play out the remainder of cycle: */
	if (self->speed != 0.0f) {
		playSong(self, self->positionInFrames+timeOffset, self->positionInFrames+sampleCount, outCapacity);

		/* positionInFrames is the point at the beginning of the cycle */
		self->positionInFrames += sampleCount - timeOffset;
	}
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
	Self* self = (Self*)instance;

printf("CALLING deactivate\n");			
//TODO cf if patterns or tracks and added dynamically. Only recalc the affected track.
	for (int t=0; t<self->numTracks; t++) 
		self->timeline->tracks[t].state = TRACK_NOT_PLAYING;

}

/*
   Destroy a plugin instance (counterpart to `instantiate()`).

   This method is in the ``instantiation'' threading class, so no other
   methods on this instance will be called concurrently with it.
*/
static void cleanup(LV2_Handle instance)
{
	Self* self = (Self*)instance;

//	free(self->patterns);
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

