#ifndef PLUGIN_LOADER_H
#define PLUGIN_LOADER_H

	#include <lilv/lilv.h>
	#include <lv2/core/lv2.h>
	#include <stdarg.h>
	#include <stdbool.h>
	#include <stdint.h>


	/* Control port value set from the command line */
	/*
	typedef struct Param {
		char* sym;   ///< Port symbol
		float value; ///< Control value
	} Param;
	*/

	typedef enum {
		START_LOOP,
		STOP_LOOP
	} LOOP_COMMAND;

	typedef struct {
		int todo1;
		int todo2;
		int todo3;
	} MidiMessage;


	/* Runtime port information */
	typedef struct {
		LilvPort* lilvPort; ///< Port description
		uint32_t index;     ///< Port index
	} Port;

	/* 
		lilv has all of these:
			LilvPlugin			- "only represents the data of the plugin"  (maybe the ttl files?)
			LilvInstance        - allows us to load and "access the actual plugin code"
			LilvPluginClass     - maybe grouping for plugins?
	*/

	/* Plugin information: */
	typedef struct {
		char* uri;
		LilvPlugin* lilvPlugin;
		LilvInstance* instance;

//XXX am I allowed to share the ports between plugins?
		Port* controlPort;
		Port* midiPortOut;

	//TODO? share this buffer between plugins. IS permitted, but how is it achieved in practice?
	//NOTE unlike the inputs we really have to allow multiple messages here.
	//	MidiMessage midiMessage[100];
	} Plugin;

	/* Application state */
	typedef struct {
		LilvWorld* world;
		int numPlugins;
		Plugin* plugins;
		LilvPlugins* lilvPlugins;
		int sampleRate;
	} Plugins; 

	#include "songEditor.h"

	bool instantiatePlugins(Self* self,Plugins* plugins);
	void activatePlugins(Plugins* plugins);
	void deactivatePlugins(Plugins* plugins);
	void cleanupPlugins(Plugins* plugins);

#endif // PLUGIN_LOADER_H
