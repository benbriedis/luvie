#include <assert.h>
#include <stdint.h>
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
				{0, 5, {2, 0.0}, 4.0, 0, 0},
				{0, 6, {6, 1.0}, 3.0, 0, 0}
			}, TRACK_NOT_PLAYING, 0 
		},
		{9,"Track 9", 2, 
			{
				{0, 3, {4, 0.0}, 4.0, 0, 0},
				{0, 4, {8, 1.0}, 3.0, 0, 0}
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
	uris->time_frame          = map->map(map->handle, LV2_TIME__frame);

	uris->loopsMessage        = map->map(map->handle, "https://github.com/benbriedis/luvie#loopsMessage");
	uris->loopId              = map->map(map->handle, "https://github.com/benbriedis/luvie#loopId");
	uris->loopEnable          = map->map(map->handle, "https://github.com/benbriedis/luvie#loopEnable");
	uris->loopStartFrame      = map->map(map->handle, "https://github.com/benbriedis/luvie#loopStartFrame");

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
	self->positionInFrames = 0;
	self->testPositionInFrames = 0;

	self->timeline = calloc(1,sizeof(timeline));
	if (!self)
		return NULL;
	memcpy(self->timeline,&timeline,sizeof(timeline));

	self->numTracks = 2;

	initPatternEndPoints(self);

	for (int t=0; t<self->numTracks; t++) 
		self->timeline->tracks[t].state = TRACK_NOT_PLAYING;

    lv2_atom_forge_init(&self->forge, self->map);

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
		case CONTROL_IN:
			self->controlPortIn = (LV2_Atom_Sequence*)data;
			break;

//TODO ==> CONTROL_OUT
		case MIDI_OUT:
			self->midiPortOut = (LV2_Atom_Sequence*)data;
			break;
	}
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


static void sendLoopOnMessage(Self* self,Plugin* plugin,int loopIndex,long numSamples,long startFrame)
{
	printf("loopIndex: %d\n",loopIndex);
	printf("numSamples: %ld\n",numSamples);
	printf("startFrame: %ld\n",startFrame);

//XXX ALTERNATIVE: call child plugin run() at the same time as ours. Requires larger buffers.

//TODO maybe try a 'Patch' or some other control message...

//XXX alternative:  lv2_atom_forge_set_sink()
	lv2_atom_forge_set_buffer(&self->forge,plugin->controlMessage,100);  //TODO replace 100. NB only one tuple sent per run() call

//NOTE one int is sneaking out OK. Maybe try catching in harmony.c run()?

printf("sendLoopOnMessage() -1 loopsMessageId: %d\n",self->uris.loopsMessage);	

	LV2_Atom_Forge_Frame frame;

//FIXME need this for the frame I think...
//	lv2_atom_forge_sequence_head(&self->forge,&frame,self->uris.time_frame);    //   unit is the URID of unit of event time stamps. 

/*
printf("sendLoopOnMessage() -1B\n");	
	LV2_Atom* tup = (LV2_Atom*)lv2_atom_forge_tuple(&self->forge, &frame);
printf("sendLoopOnMessage() -1C\n");	
	lv2_atom_forge_int(&self->forge, loopIndex);
printf("sendLoopOnMessage() -1D\n");	
	lv2_atom_forge_int(&self->forge, START_LOOP);
printf("sendLoopOnMessage() -1E\n");	
	lv2_atom_forge_int(&self->forge, startFrame);
printf("sendLoopOnMessage() -2\n");	
	lv2_atom_forge_pop(&self->forge, &frame); //XXX OR does the sequence look after this?
*/	
 
	lv2_atom_forge_object(&self->forge, &frame,0,self->uris.loopsMessage); // 0 = "a blank ID"
	lv2_atom_forge_key(&self->forge, self->uris.loopId);
	lv2_atom_forge_int(&self->forge, loopIndex);
	lv2_atom_forge_key(&self->forge, self->uris.loopEnable);
	lv2_atom_forge_bool(&self->forge, true);
	lv2_atom_forge_key(&self->forge, self->uris.loopStartFrame);
	lv2_atom_forge_long(&self->forge, startFrame);
	lv2_atom_forge_pop(&self->forge, &frame); //XXX OR does the sequence look after this?
 

printf("sendLoopOnMessage() -3\n");	

	lilv_instance_run(plugin->instance,numSamples);

/* NOTE we CAN reuse the loopsMessage buffer between plugins.
   We would need to ensure same pattern IDs are used between plugins.
		See https://lv2plug.in/c/html/group__lv2core.html#a3cb9de627507db42e338384ab945660e - connect_port()
*/

printf("sendLoopOnMessage() END\n");	
}

//XXX child plugins dont need to support time position. Song editor can look after that.

static void sendLoopOffMessage(Self* self,Plugin* plugin,int loopIndex,long numSamples)
{
	lv2_atom_forge_set_buffer(&self->forge,&plugin->controlMessage,100);  //replace 100

	LV2_Atom_Forge_Frame frame;
//	lv2_atom_forge_sequence_head(&self->forge,&frame,self->uris.time_frame);
	LV2_Atom* tup = (LV2_Atom*)lv2_atom_forge_tuple(&self->forge, &frame);
	lv2_atom_forge_int(&self->forge, loopIndex);
	lv2_atom_forge_int(&self->forge, STOP_LOOP);
	lv2_atom_forge_pop(&self->forge, &frame); //XXX OR does the sequence look after this?

	lilv_instance_run(plugin->instance,numSamples);
}


static void playTrack(Self* self, Track* track, long cyclePos, long absBegin, long absEnd, uint32_t outCapacity)
{
	long pos = absBegin;

	/* In theory a cycle can contain many note changes. Best to support it, however unlikely and undesirable. */
	for (int pat=track->currentOrNext; pos<absEnd && pat<track->numPatterns; pat++) {

		Pattern* pattern = &track->patterns[track->currentOrNext];
		Plugin* plugin = &self->plugins.plugins[pattern->pluginIndex];

		/* NOTE a track can only play one pattern at a time */

		if (track->state == TRACK_PLAYING && pos >= pattern->endInFrames) {
			printf("TURNING OFF PATTERN       TRACK: %d  PATTERN: %d\n",track->id,pattern->id);

			sendLoopOffMessage(self,plugin,pattern->id,absEnd - absBegin);

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
sendLoopOnMessage(self,plugin,pattern->id,absEnd - absBegin,0);
//			sendLoopOnMessage(self,plugin,pattern->id,absEnd - absBegin,cyclePos);

			printf("SENT MESSAGE A\n");

			track->state = TRACK_PLAYING;
			pos = pattern->startInFrames;
		}
	}
}

/* Play patterns in the range [begin..end) relative to this cycle.  */
static void playSong(Self* self, long cyclePos, long absBegin, long absEnd, uint32_t outCapacity)
{
//TODO add mute and solo features for tracks

	for (int t=0; t < self->numTracks; t++) {
		Track* track = &self->timeline->tracks[t];

		playTrack(self,track,cyclePos,absBegin,absEnd,outCapacity);
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
			printf("TRANSPORT JUMPED\n");
			adjustNextPatternIndexes(self,newPos);
		}
	}
	else if (self->speed==1.0f && newSpeed==1.0f) {
		if (newPos != self->positionInFrames + offsetFrames)
			printf("TRANSPORT JUMPED WHILE TRANSPORT RUNNING\n");
			//TODO Just log a warning if this happens
	}

	self->positionInFrames = newPos;


printf("\n");

//FIXME the speed (and frame) guard needs to be earlier to use useful...	
	/* Speed=0 (stop) or speed=1 (play) */
	if (speed && speed->type == uris->atom_Float) 
		self->speed = newSpeed;
}

//XXX another use case muting or soloing a track should probably instantly start or stop the current pattern

/*
	`run()` must be real-time safe. No memory allocations or blocking!
	Note the spec says that sampleCount=0 must be supported for latency updates etc.
*/
static void run(LV2_Handle instance, uint32_t sampleCount)
{
	Self* self = (Self*)instance;
 	const URIs* uris = &self->uris;

  	/* Initially self->out_port contains a Chunk with size set to capacity */
	const uint32_t outCapacity = self->midiPortOut->atom.size;

	/* Write an empty Sequence header to the output */
	lv2_atom_sequence_clear(self->midiPortOut);
	self->midiPortOut->atom.type = self->uris.atom_Sequence;

	/* Loop through events: */
	const LV2_Atom_Sequence* in = self->controlPortIn;
	uint32_t lastPos = 0;


//XXX dont have an output port to get this capacity from...
//const uint32_t capacity = self->connectPort->atom.size;

//TODO for every plugin...
	for (int i=0; i<self->plugins.numPlugins; i++) {
		Plugin* plugin = &self->plugins.plugins[i];
//		lv2_atom_forge_sequence_head(&self->forge, &self->notify_frame, 0);
	}



//XXX cf 'handlePartCycle()'
int i=0;	
	LV2_ATOM_SEQUENCE_FOREACH (self->controlPortIn, ev) {
		/* pos is usually 0 - except for when a position message comes part way through a cycle. */
		uint32_t pos = ev->time.frames;

		// Play the click for the time slice from last_t until now
		if (self->speed != 0.0f) 
			playSong(self,pos,self->positionInFrames+lastPos, self->positionInFrames+pos,outCapacity);

printf("cyclePos: %d\n",pos);	//ARDOUR USUALLY RETURNS 0 HERE, 
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
				maybeJump(self, obj,pos);
			}
			else
printf("GOT A DIFFERENT TYPE OF MESSAGE  otype: %d\n",obj->body.otype);

//TODO probably accept another atom type message to change pattern indexes (and maybe allow patterns to be added or removed).
//     Maybe a CC or CV message. MIDI is maybe a possibility. Otherwise custom.
		}

if (i!=0)
printf("i: %d\n",i);				

//XXX Q: should we calling this 'play' for all message types? 

		lastPos = pos;
i++;		
	}

	/* Play out the remainder of cycle: */
	if (self->speed != 0.0f) {
		playSong(self,sampleCount,self->positionInFrames+lastPos, self->positionInFrames+sampleCount, outCapacity);

		/* positionInFrames is the point at the beginning of the cycle */
		self->positionInFrames += sampleCount - lastPos;
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

