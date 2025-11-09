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
	int loopIndex;
	LOOP_COMMAND command;
	long startFrame;
} LoopMessage;


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
	Port* controlPort;
//XXX or allow for multiple messages?
	LoopMessage* message;
} Plugin;

/* Application state */
typedef struct {
	LilvWorld* world;
	int numPlugins;
	Plugin* plugins;
	LilvPlugins* lilvPlugins;
	int sampleRate;
} Plugins; 


bool instantiatePlugins(Plugins* self);
void runPlugins(Plugins* self);
void activatePlugins(Plugins* self);
void deactivatePlugins(Plugins* self);
void cleanupPlugins(Plugins* self);
