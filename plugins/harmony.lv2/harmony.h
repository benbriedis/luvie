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
	#include <stdint.h>

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
		int enabled;

		uint32_t position; 

	//TODO generalise the number of notes
		Note notes[5];
	//TODO probably add a MIDI channel here
	} Pattern;

	//TODO replace NOTE_OFF and NOTE_ON with boolean too I think

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
		const LV2_Atom_Sequence* controlInBuffer;
		LV2_Atom_Sequence* midiOutBuffer;

		URIs uris;   // Cache of mapped URIDs

		// Variables to keep track of the tempo information sent by the host
		double sampleRate; 
		float bpm;
		float speed; // Transport speed (usually 0=stop, 1=play)
		Pattern* patterns;
		int numPatterns;
	} Self;

#endif
