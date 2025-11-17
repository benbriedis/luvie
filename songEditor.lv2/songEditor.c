#include <assert.h>
#include <lv2/atom/atom.h>
#include <lv2/urid/urid.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
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

	/* NOTE lv2_atom_forge_init() may notbe runtime safe, so keep out of run() (see API docs) */
    lv2_atom_forge_init(&self->controlForge, self->map);


	//XXX TEST of State extension XXX
	self->uris.atom_String = map->map(map->handle, LV2_ATOM__String);
	self->uris.myGreeting = map->map(map->handle, PLUGIN_URI "greeting");
	self->state.greeting = strdup("Hello");

printf("Finished instantiate()\n");	
	return (LV2_Handle)self;
}


LV2_State_Status my_save(LV2_Handle instance,LV2_State_Store_Function store,LV2_State_Handle handle,uint32_t flags,const LV2_Feature *const *features)
{
printf("my_save() CALLED!\n");
    Self* self = (Self*)instance;
    const char* greeting = self->state.greeting;

    store(handle,self->uris.myGreeting, greeting, strlen(greeting) + 1, self->uris.atom_String, LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);

    return LV2_STATE_SUCCESS;
}

LV2_State_Status my_restore(LV2_Handle instance, LV2_State_Retrieve_Function retrieve, LV2_State_Handle handle, uint32_t flags, const LV2_Feature *const * features)
{
printf("my_restore() CALLED!\n");
    Self* self = (Self*)instance;

    size_t size;
    uint32_t type;
//    uint32_t flags;
    const char* greeting = retrieve(handle, self->uris.myGreeting, &size, &type, &flags);

    if (greeting) {
        free(self->state.greeting);
        self->state.greeting = strdup(greeting);
    } else 
        self->state.greeting = strdup("Hello");

    return LV2_STATE_SUCCESS;
}

/*
   The plugin must store the data location, but data may not be accessed except in run().
   This method is in the 'audio' threading class, and is called in the same context as run().
*/
static void connectPort(LV2_Handle instance, uint32_t port, void* data)
{
//XXX looks like its meant to called over and over

	Self* self = (Self*)instance;

	switch (port) {
//XXX cf ==> 'TIME_IN'		
		case CONTROL_IN:
			self->timePositionBuffer = (LV2_Atom_Sequence*)data;
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


static void handleTrack(Self* self, Track* track,long absPos,int last,int current)
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
static void createLoopCommands(Self* self,long absPos,int last,int current)
{
//TODO add mute and solo features for tracks

	for (int t=0; t < self->numTracks; t++) {
		Track* track = &self->timeline->tracks[t];

		handleTrack(self,track,absPos,last,current);
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
	/*
		Ardour seems to take a while to establish itself on play. Possibly addressing latency issues. You even get negative frames.
		The frames seem to be OK and aligned once speed=1.

		Stopping and start in Ardour appears to jump back 3 "cycles" (eg 3 x 1024frames).
	*/

	LV2_Atom* speed = NULL;
	LV2_Atom* time_frame = NULL;

	lv2_atom_object_get(obj,
		self->uris.time_speed, &speed,
		self->uris.time_frame, &time_frame,
		NULL
	);

	/* I believe newPos incorporates the offsetFrames */
	long newPos = ((LV2_Atom_Long*)time_frame)->body;
	float newSpeed = ((LV2_Atom_Float*)speed)->body;

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

//FIXME the speed (and frame) guard needs to be earlier to use useful...	
	/* Speed=0 (stop) or speed=1 (play) */
	if (speed && speed->type == self->uris.atom_Float) 
		self->speed = newSpeed;
}

static void createLoopCommandsAndHandleJumps(Self* self,uint32_t* pos)
{							 
	LV2_ATOM_SEQUENCE_FOREACH (self->timePositionBuffer, ev) {
		uint32_t offset = ev->time.frames;

		if (self->speed != 0.0f) 
			createLoopCommands(self,self->absolutePosition,pos,offset);

		if (ev->body.type == self->uris.atom_Object || ev->body.type == self->uris.atom_Blank) {
			const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&ev->body;

			if (obj->body.otype == self->uris.time_Position) 
				maybeJump(self, obj,offset);
			else
printf("GOT A DIFFERENT TYPE OF MESSAGE  otype: %d\n",obj->body.otype);
		}

		*pos = offset;
	}
}

static void processInput(Self* self, uint32_t sampleCount)
{
	/* Write an empty Sequence header to the output */
	lv2_atom_sequence_clear(self->controlBuffer);
	self->midiOutBuffer->atom.type = self->uris.atom_Sequence; 
	self->midiOutBuffer->body.unit = self->uris.time_frame;

	lv2_atom_forge_set_buffer(&self->controlForge,(uint8_t*)&self->controlBuffer,sizeof(self->controlBuffer));  //TODO ensure size not exceeded. NB only one tuple sent per run() call
	LV2_Atom_Forge_Frame controlSequenceFrame; 
	lv2_atom_forge_sequence_head(&self->controlForge,&controlSequenceFrame,self->uris.time_frame);  

	/* Loop through the input events: */
	uint32_t pos = 0;
	createLoopCommandsAndHandleJumps(self,&pos);

	/* Play out the remainder of cycle: */
	if (self->speed != 0.0f) 
		createLoopCommands(self,self->absolutePosition,pos,sampleCount);

	/* Update the absolute position: */
	self->absolutePosition += sampleCount - pos;

	/* Complete the sequence: */
	lv2_atom_forge_pop(&self->controlForge, &controlSequenceFrame);
}

static void runPlugins(Self* self,uint32_t sampleCount)
{
	/* Prepare MIDI output: */
	lv2_atom_sequence_clear(self->midiOutBuffer);
	self->midiOutBuffer->atom.type = self->uris.atom_Sequence; 
	self->midiOutBuffer->body.unit = self->uris.time_frame;

	/* Call our plugins: */
	for (int i=0; i<self->plugins.numPlugins; i++) {
		Plugin* plugin = &self->plugins.plugins[i];
		lilv_instance_run(plugin->instance,sampleCount);
	}
}

/*
	REMEMBER No memory allocations or blocking!
*/
static void run(LV2_Handle instance, uint32_t sampleCount)
{
	processInput((Self*)instance,sampleCount);
	runPlugins((Self*)instance,sampleCount);
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
static const void* extensionData(const char* uri)
{
printf("In extensionData() uri: %s\n",uri);	
	static const LV2_State_Interface state_iface = { my_save, my_restore };
	if (!strcmp(uri, LV2_STATE__interface)) 
{		
printf("In extensionData() returning state_iface\n");	

//XXX Q: am I meant to put something in the .ttl file to request this? Ardour seems to poll it anyway

		return &state_iface;
}
printf("In extensionData() returning NULL\n");	

	return NULL;
}

/* Every plugin must define an `LV2_Descriptor`.  It is best to define descriptors statically. */
static const LV2_Descriptor descriptor = {PLUGIN_URI, instantiate, connectPort, activate, 
	run, deactivate, cleanup, extensionData};

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
	return index == 0 ? &descriptor : NULL;
}


//TODO later test that the output is all in time

