#include <assert.h>
#include <lv2/atom/atom.h>
#include <lv2/urid/urid.h>
#include <stdint.h>
#include <string.h>
#include "pluginLoader.h"
#include "songEditor.h"
#include "lv2/atom/forge.h"

//TODO use these time signatures and bpms
Timeline timeline = {
	{{0,0,0}},
	{{{140}}},
	{
		{7,"Track 8", 2, 
			{
				{0, 0, {2, 0.0}, 4.0, 0, 0},
				{0, 1, {6, 1.0}, 3.0, 0, 0}
			}, TRACK_NOT_PLAYING, 0 
		},
		{9,"Track 9", 2, 
			{
				{0, 1, {4, 0.0}, 4.0, 0, 0},
				{0, 0, {8, 1.0}, 3.0, 0, 0}
			}, TRACK_NOT_PLAYING, 0 
		},
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

	LV2_URID_Unmap* unmap; //TODO delete

	/* Scan host features for URID map */
	const char* missing = lv2_features_query(
		features,
		LV2_LOG__log, &self->logger.log, false,
		LV2_URID__map, &self->map, true,
		LV2_URID__unmap, &unmap, true,
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
	uris->time_frame          = map->map(map->handle, LV2_TIME__frame);

	uris->loopsMessage        = map->map(map->handle, "https://github.com/benbriedis/luvie#loopsMessage");
	uris->loopId              = map->map(map->handle, "https://github.com/benbriedis/luvie#loopId");
	uris->loopEnable          = map->map(map->handle, "https://github.com/benbriedis/luvie#loopEnable");
	uris->loopStartFrame      = map->map(map->handle, "https://github.com/benbriedis/luvie#loopStartFrame");

	/*
printf("MAP\tatom blank: %d\n",uris->atom_Blank);
printf("MAP\tatom float: %d\n",uris->atom_Float);
printf("MAP\tatom object: %d\n",uris->atom_Object);
printf("MAP\tatom path: %d\n",uris->atom_Path);
printf("MAP\tatom resource: %d\n",uris->atom_Resource);
printf("MAP\tatom sequence: %d\n",uris->atom_Sequence);
printf("MAP\tatom URID: %d\n",uris->atom_URID);
printf("MAP\tatom eventTransfer: %d\n",uris->atom_eventTransfer);
printf("MAP\tmidi event: %d\n",uris->midi_Event);
printf("MAP\tpatch set: %d\n",uris->patch_Set);
printf("MAP\tpatch property: %d\n",uris->patch_property);
printf("MAP\tpatch value: %d\n",uris->patch_value);
printf("MAP\ttime position: %d\n",uris->time_Position);
printf("MAP\ttime speed: %d\n",uris->time_speed);
printf("MAP\ttime frame: %d\n",uris->time_frame);
printf("MAP\tloops message: %d\n",uris->loopsMessage);
printf("MAP\tloops id: %d\n",uris->loopId);
printf("MAP\tloops enable: %d\n",uris->loopEnable);
printf("MAP\tloops startFrame: %d\n",uris->loopStartFrame);

printf("UMMAP\t2 is: %s\n",unmap->unmap(map->handle,2));
printf("UMMAP\t32 is: %s\n",unmap->unmap(map->handle,32));
printf("UMMAP\t35 is: %s\n",unmap->unmap(map->handle,35));
printf("UMMAP\t0 is: %s\n",unmap->unmap(map->handle,0));
*/

//////////////////

	self->plugins.sampleRate = sampleRate;

	//XXX hack? will just pass through our features to our kids for the moment
	self->features = features;

	instantiatePlugins(self,&self->plugins);

printf("Back from addPlugins\n");	

//////////////////
	

	/* Initialise instance data: */
	self->sampleRate = sampleRate;
	self->bpm = 120.0f;
	self->absolutePosition = 0;
	self->testPosition = 0;

	self->timeline = calloc(1,sizeof(timeline));
	if (!self)
		return NULL;
	memcpy(self->timeline,&timeline,sizeof(timeline));

	self->numTracks = 2;

	initPatternEndPoints(self);

	for (int t=0; t<self->numTracks; t++) 
		self->timeline->tracks[t].state = TRACK_NOT_PLAYING;

    lv2_atom_forge_init(&self->controlForge, self->map);
    lv2_atom_forge_init(&self->midiForge, self->map);

printf("Finished instantiate()\n");	
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
//XXX cf ==> 'TIME_IN'		
		case CONTROL_IN:
			self->controlInBuffer = (LV2_Atom_Sequence*)data;
			break;

		case MIDI_OUT:
			self->midiOutBuffer = (LV2_Atom_Sequence*)data;
			break;
	}

	connectPorts(self,&self->plugins);
}

/* The plugin must reset all internal state */
static void activate(LV2_Handle instance)
{
printf("CALLING songEditor activate() - START\n");			

	Self* self = (Self*)instance;

	/* NOTE positionInFrames shouldn't be set here */

	activatePlugins(&self->plugins);

printf("CALLING songEditor activate() - END\n");			
}

//static void sendMessage()
//{
	/* Updates:
		[
			{id:4, state:on/off, offset:inFrames}, 
			{id:6, state:on/off, offset:inFrames}
		]


		Also cf add pattern, delete pattern
	*/

//}

//TODO can we use logger? 


static void sendLoopOnMessage(Self* self,int patternId,long patternOffset)
{
/* TODO probably use a sequence of Patch.set instead to maximise reusabilty.  cf	

    a patch:Set ;
    patch:subject "Pattern5" ;
    patch:property "enabled" ;
    patch:value true .

	patch:context "offset: 500" .   *** Unclear about this bit ***

cf also using LV2 Parameters (https://lv2plug.in/ns/ext/parameters.html#ControlGroup)
   - Maybe add a group?
   - + an enabled setting (or use bypass)
*/	


	printf("loopIndex: %d\n",patternId);
	printf("patternOffset: %ld\n",patternOffset);
	printf("sendLoopOnMessage() -1 loopsMessageId: %d\n",self->uris.loopsMessage);	

//XXX could/should probably use THIS time frame rather than the one below? 
//    NO -> one is relative to run(), the other to the pattern

	LV2_Atom_Forge_Frame frame;
    lv2_atom_forge_frame_time(&self->controlForge,0);
	lv2_atom_forge_object(&self->controlForge, &frame,0,self->uris.loopsMessage); // 0 = "a blank ID"
	lv2_atom_forge_key(&self->controlForge, self->uris.loopId);
	lv2_atom_forge_int(&self->controlForge, patternId);
	lv2_atom_forge_key(&self->controlForge, self->uris.loopEnable);
	lv2_atom_forge_bool(&self->controlForge, true);
	lv2_atom_forge_key(&self->controlForge, self->uris.loopStartFrame);
//TODO rename to 'offset' I think. Can start at 0 or at the point relative to the current pattern.
	lv2_atom_forge_long(&self->controlForge, patternOffset);
	lv2_atom_forge_pop(&self->controlForge, &frame); 
printf("sendLoopOnMessage() END\n\n");	
}

//XXX child plugins dont need to support time position. Song editor can look after that.

static void sendLoopOffMessage(Self* self,int patternId)
{
	LV2_Atom_Forge_Frame frame;
    lv2_atom_forge_frame_time(&self->controlForge,0);
	lv2_atom_forge_object(&self->controlForge, &frame,0,self->uris.loopsMessage); // 0 = "a blank ID"
	lv2_atom_forge_key(&self->controlForge, self->uris.loopId);
	lv2_atom_forge_int(&self->controlForge, patternId);
	lv2_atom_forge_key(&self->controlForge, self->uris.loopEnable);
	lv2_atom_forge_bool(&self->controlForge, false);
	lv2_atom_forge_pop(&self->controlForge, &frame); 
printf("sendLoopOffMessage() END\n\n");	
}


static void playTrack(Self* self, Track* track,long absPos,int last,int current)
{
	long pos = absPos + last;

	/* In theory a cycle can contain many note changes. Best to support it, however unlikely and undesirable. */
	for (int pat=track->currentOrNext; pos<absPos+current && pat<track->numPatterns; pat++) {

		Pattern* pattern = &track->patterns[track->currentOrNext];

		/* NOTE a track can only play one pattern at a time */

		if (track->state == TRACK_PLAYING && pos >= pattern->endInFrames) {
			printf("TURNING OFF PATTERN       TRACK: %d  PATTERN: %d\n",track->id,pattern->id);

			sendLoopOffMessage(self,pattern->id);

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
			printf("TURNING ON PATTERN     TRACK: %d  PATTERN: %d\n",track->id,pattern->id);

//FIXME want the start, cyclePos is the current position (ie more like the end)			
			sendLoopOnMessage(self,pattern->id,absPos + current - pattern->startInFrames);

			printf("SENT MESSAGE A\n");

			track->state = TRACK_PLAYING;
			pos = pattern->startInFrames;
		}
	}
}

/* Play patterns in the range [lastPos..currentPos) relative to this cycle.  */
static void playSong(Self* self,long absPos,int last,int current)
{
//TODO add mute and solo features for tracks

	for (int t=0; t < self->numTracks; t++) {
		Track* track = &self->timeline->tracks[t];

		playTrack(self,track,absPos,last,current);
	}
}

static void adjustNextPatternIndexes(Self* self,long newPos)
{
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
		Ardour seems to take a while to establish itself on play. Possibly addressing latency issues. You even get negative frames.
		The frames seem to be OK and aligned once speed=1.

		Stopping and start in Ardour appears to jump back 3 "cycles" (eg 3 x 1024frames).
	*/

	LV2_Atom* speed = NULL;
	LV2_Atom* time_frame = NULL;

	lv2_atom_object_get(obj,
		uris->time_speed, &speed,
		uris->time_frame, &time_frame,
		NULL
	);

	/* I believe newPos incorporates the offsetFrames */
	long newPos = ((LV2_Atom_Long*)time_frame)->body;
	float newSpeed = ((LV2_Atom_Float*)speed)->body;

	/*
if (speed) 
printf("speed: %f\n",newSpeed);

if (time_frame) 
printf("frame: %ld\n",newPos);

printf("positionInFrames: %ld\n",self->absolutePosition);
printf("offsetFrames: %d\n",offsetFrames); 
*/


	if (self->speed==1.0f && newSpeed==0.0f) {
		self->testPosition = newPos;

		for (int t=0; t<self->numTracks; t++) {
			self->timeline->tracks[t].state = TRACK_NOT_PLAYING;
			//TODO send stop message to child plugins
		}
	}
	else if (self->speed==0.0f && newSpeed==1.0f) {
		if (newPos != self->testPosition + offsetFrames) {
			printf("TRANSPORT JUMPED\n");
			adjustNextPatternIndexes(self,newPos);
		}
	}
	else if (self->speed==1.0f && newSpeed==1.0f) {
		if (newPos != self->absolutePosition + offsetFrames)
			printf("TRANSPORT JUMPED WHILE TRANSPORT RUNNING\n");
			//TODO Just log a warning if this happens
	}

	self->absolutePosition = newPos;


//printf("\n");

//FIXME the speed (and frame) guard needs to be earlier to use useful...	
	/* Speed=0 (stop) or speed=1 (play) */
	if (speed && speed->type == uris->atom_Float) 
		self->speed = newSpeed;
}

//TODO delete after testing...
typedef struct {
	LV2_Atom_Event event;
	uint8_t msg[3];
} MIDINoteEvent;

//XXX another use case muting or soloing a track should probably instantly start or stop the current pattern

/*
	`run()` must be real-time safe. No memory allocations or blocking!
	Note the spec says that sampleCount=0 must be supported for latency updates etc.
*/
static void run(LV2_Handle instance, uint32_t sampleCount)
{
	Self* self = (Self*)instance;
 	const URIs* uris = &self->uris;

	/* Write an empty Sequence header to the output */
//	lv2_atom_sequence_clear(self->midiOutBuffer);
//	self->midiOutBuffer->atom.type = self->uris.atom_Sequence;

	/* --- Create pattern messages and handle time position changes --- */


	/* Prepare for creation of pattern messages: */
//FIXME is this what people do? Presumably only need to set 0s at the start of the buffer...
	memset(&self->controlBuffer,0,sizeof(self->controlBuffer));
//XXX I image sequence_head does this... TODO probably delete   NOT WORKING
//	lv2_atom_sequence_clear((LV2_Atom_Sequence*)&self->controlMessage);
//XXX doesnt work either:
//	((LV2_Atom_Sequence*)(&self->controlBuffer))->atom.size = 0;
//	((LV2_Atom_Sequence*)(&self->controlBuffer))->atom.type = 0;


//XXX alternative:  lv2_atom_forge_set_sink()
	lv2_atom_forge_set_buffer(&self->controlForge,(uint8_t*)&self->controlBuffer,sizeof(self->controlBuffer));  //TODO ensure size not exceeded. NB only one tuple sent per run() call
	LV2_Atom_Forge_Frame controlSequenceFrame; 
	lv2_atom_forge_sequence_head(&self->controlForge,&controlSequenceFrame,self->uris.time_frame);    //   unit is the URID of unit of event time stamps. 


//XXX cf separate into handleEvents() or handleEvent()...
	/* Loop through events: */
	uint32_t pos = 0;

	LV2_ATOM_SEQUENCE_FOREACH (self->controlInBuffer, ev) {
		uint32_t offset = ev->time.frames;

		// Play the click for the time slice from last_t until now
		if (self->speed != 0.0f) 
			playSong(self,self->absolutePosition,pos,offset);

		/* 
			NOTE position messages sometimes come part way through a cycle (eg 1024 frames).
			In this case the Position frames includes the start of the standard cycle.
		*/

//		if (lv2_atom_forge_is_object_type(const LV2_Atom_Forge *forge, uint32_t type))  XXX Use this in future if we create a forge
		if (ev->body.type == uris->atom_Object || ev->body.type == uris->atom_Blank) {
			const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&ev->body;

			if (obj->body.otype == uris->time_Position) 
				maybeJump(self, obj,offset);
			else
printf("GOT A DIFFERENT TYPE OF MESSAGE  otype: %d\n",obj->body.otype);

//TODO probably accept another atom type message to change pattern indexes (and maybe allow patterns to be added or removed).
//     Maybe a CC or CV message. MIDI is maybe a possibility. Otherwise custom.
		}

//XXX Q: should we calling this 'play' for all message types? 

		pos = offset;
	}

	/* Play out the remainder of cycle: */
	if (self->speed != 0.0f) 
		playSong(self,self->absolutePosition,pos,sampleCount);

	/* Update the absolute position: */
	self->absolutePosition += sampleCount - pos;

	/* Complete the sequence: */
	lv2_atom_forge_pop(&self->controlForge, &controlSequenceFrame);


	/* --- Call our plugins --- */

	/* Prepare for MIDI output: */
//FIXME is this what people do? Presumably only need to set 0s at the start of the buffer...
//	memset(&self->midiBuffer,0,sizeof(self->midiBuffer));

	/*
//XXX XXX PROBABLY CANT / SHOULDNT USE FORGE HERE... CANT REALLY SHARE BETWEEN PLUGINS	
	lv2_atom_forge_set_buffer(&self->midiForge,(uint8_t*)&self->midiBuffer,sizeof(self->midiBuffer));
	LV2_Atom_Forge_Frame midiSequenceFrame; 
	lv2_atom_forge_sequence_head(&self->midiForge,&midiSequenceFrame,self->uris.time_frame);
    lv2_atom_forge_frame_time(&self->midiForge,0);
	*/

	// 1. add sequence head
	// 2. add frame time  (Q where are frame times really meant to be? Starts of sequences or in front of each member atom/event, or both?)
//TODO delete midiBuffer... should be using midiOutBuffer...	
//	void* buf = &self->midiBuffer;
/*
	((LV2_Atom_Sequence*)buf)->atom.size = sizeof(LV2_Atom_Sequence_Body);
	((LV2_Atom_Sequence*)buf)->atom.type = self->uris.atom_Sequence;
	((LV2_Atom_Sequence*)buf)->body.unit = self->uris.time_frame;
	((LV2_Atom_Sequence*)buf)->body.pad = 0;
*/	

	//lv2_atom_sequence_clear(self->midiOutBuffer);
	self->midiOutBuffer->atom.size = sizeof(LV2_Atom_Sequence_Body);
	self->midiOutBuffer->atom.type = self->uris.atom_Sequence; //XXX do we need these?
	self->midiOutBuffer->body.unit = self->uris.time_frame;

	/*
//XXX ADD A TEST NOTE...
	const uint32_t outCapacity = self->midiOutBuffer->atom.size; //TODO reconsider
	MIDINoteEvent out;
	out.event.time.frames = 50;
	out.event.body.type = self->uris.midi_Event;
	out.event.body.size = 3;
	out.msg[0] = 0x90; 
	out.msg[1] = 60;       
	out.msg[2] = 100;   
	lv2_atom_sequence_append_event(self->midiOutBuffer,outCapacity, &out.event);
	*/


//TEST try adding a MIDI note ourselves before calling the plugin. Survive does it?

	/* Call our plugins: */
	for (int i=0; i<self->plugins.numPlugins; i++) {
		Plugin* plugin = &self->plugins.plugins[i];
		lilv_instance_run(plugin->instance,sampleCount);
	}

//TODO close off the sequence
//	lv2_atom_forge_pop(&self->midiForge, &midiSequenceFrame);

	//3. End sequence if necessary
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

	deactivatePlugins(&self->plugins);
}

/*
   Destroy a plugin instance (counterpart to `instantiate()`).

   This method is in the ``instantiation'' threading class, so no other
   methods on this instance will be called concurrently with it.
*/
static void cleanup(LV2_Handle instance)
{
	Self* self = (Self*)instance;

	cleanupPlugins(&self->plugins);

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

