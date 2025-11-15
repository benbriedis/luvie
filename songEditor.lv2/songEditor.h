#ifndef SONG_EDITOR_H
#define SONG_EDITOR_H

	#include <lv2/atom/atom.h>
	#include <lv2/atom/util.h>
	#include <lv2/core/lv2.h>
	#include <lv2/core/lv2_util.h>
	#include <lv2/log/logger.h>
	#include <lv2/midi/midi.h>
	#include <lv2/patch/patch.h>
	#include <lv2/time/time.h>
	#include <lv2/urid/urid.h>
	#include <lv2/atom/forge.h>
	#include <stdint.h>
	#include <threads.h>
	#include "pluginLoader.h"

	#define PLUGIN_URI "https://github.com/benbriedis/luvie/songEditor"

	typedef enum {
		NOTE_ON,
		NOTE_OFF,
	} State;

	typedef enum {
		PATTERN_OFF,
		PATTERN_LIMITED, 
		PATTERN_UNLIMITED   //XXX probably just want PATTERN_ON now
	} PatternState;			//XXX combine with LOOP_COMMAND in pluginLoader.h?

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
		int pluginIndex;
		int id;
		//TODO probably add label here, not in luvie.c...
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
		Track tracks[2];
	} Timeline;


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
		LV2_Atom_Sequence* midiPortOut;

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

		Plugins plugins;


	//	LV2_Atom_Forge_Frame notify_frame; ///< Cached for worker replies
		LV2_Atom_Forge forge; 

		//XXX hack - just passing our features through to our children for the moment
		LV2_Feature* features;
	} Self;

#endif // SONG_EDITOR_H
