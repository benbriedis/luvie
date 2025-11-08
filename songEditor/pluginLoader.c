#include <lilv/lilv.h>
#include <lv2/core/lv2.h>

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Control port value set from the command line */
typedef struct Param {
	char* sym;   ///< Port symbol
	float value; ///< Control value
} Param;

/* Port type (only float ports are supported) */
typedef enum { TYPE_CONTROL } PortType;

/* Runtime port information */
typedef struct {
	LilvPort* lilv_port; ///< Port description
	PortType type;      ///< Datatype
	uint32_t index;     ///< Port index
	float value;     ///< Control value (if applicable)
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
	LilvNode* uriObj;
	LilvPlugin* lilvPlugin;
	LilvInstance* instance;
	unsigned numParams;
	Param* params;
	unsigned numPorts;
	Port* ports;
} Plugin;

/* Application state */
typedef struct {
	LilvWorld* world;
	int numPlugins;
	Plugin* plugins;
	LilvPlugins* lilvPlugins;
} App; 


/* Clean up all resources. */
static int cleanup(int status, App* self)
{
	/* TODO
	lilv_instance_free(self->instance);
	lilv_world_free(self->world);
	free(self->ports);
	free(self->params);
	return status;
	*/
}

/* Create port structures from data (via create_port()) for all ports. */
static int createPorts(App* self,Plugin* plugin)
{
	const LilvPlugin* lilvPlugin = plugin->lilvPlugin;

	const uint32_t numPorts = lilv_plugin_get_num_ports(lilvPlugin);
	plugin->numPorts = numPorts;
	plugin->ports   = (Port*)calloc(numPorts, sizeof(Port));

	/* Get default values for all ports */
	float* values = (float*)calloc(numPorts, sizeof(float));
	lilv_plugin_get_port_ranges_float(lilvPlugin, NULL, NULL, values);

	LilvNode* lv2_InputPort   = lilv_new_uri(self->world, LV2_CORE__InputPort);

//TODO save the control port in its own special place...

	for (uint32_t i = 0; i < numPorts; ++i) {
		Port* port  = &plugin->ports[i];
		const LilvPort* lport = lilv_plugin_get_port_by_index(lilvPlugin, i);

		port->lilv_port = lport;
		port->index = i;
		port->value = isnan(values[i]) ? 0.0f : values[i];

printf("PORT %u\n",i);
printf("IS INPUT PORT %b\n",lilv_port_is_a(lilvPlugin, lport, lv2_InputPort));


char feature[100]; //TODO make this stuff more robust. cf strcat
//XXX xf #patterns + #patterns-inputPort  / #loops + #loopsInputPort

//sprintf(feature,"%s%s",plugin->uri,"#patterns");
sprintf(feature,"%s%s","https://github.com/benbriedis/luvie","#patterns");  //FIXME
LilvNode* uriObj = lilv_new_uri(self->world, feature);
printf("feature: %s\n",feature);

printf("DOES SUPPORT EVENT %b\n",lilv_port_supports_event(lilvPlugin,lport,uriObj));
lilv_node_free(uriObj);

		/* Look for our loop/pattern input control port: */
		if (lilv_port_is_a(lilvPlugin, lport, lv2_InputPort) &&
			lilv_port_supports_event(lilvPlugin,lport,plugin->uriObj)
		) {
//TODO also check it supports patterns/loops?			
			printf("GOT OUR CONTROL PORT on %u\n",i);
//TODO store it			
//			port->type = TYPE_CONTROL;

//TODO delete in struct
		}

		LilvNode* symbol = lilv_port_get_symbol(lilvPlugin,lport);
		printf("symbol: %s\n",lilv_node_as_string(symbol));

		LilvNode* name = lilv_port_get_name(lilvPlugin,lport);
		printf("name: %s\n",lilv_node_as_string(name));
		lilv_node_free(name);

//XXX want our child to have a control input port so we can give it pattern on/off events
//    We probably also need to capture and midi-on and bundle it up in our midi output. In time 
//    maybe similar for audio and possibly control ports.

	}

printf("Started freeing \n");
	lilv_node_free(lv2_InputPort);
	free(values);

	return true;
}

static bool addPlugin(App* self,Plugin* plugin)
{
printf("addPlugin()  - A\n");

	/* Get plugin */
//XXX cf adding to plugin and freeing at the end ... (also used in ports)	
//TODO store uri in plugin too...

	plugin->uriObj = lilv_new_uri(self->world, plugin->uri);

printf("addPlugin()  - A1\n");

	const LilvPlugin* lilvPlugin = lilv_plugins_get_by_uri(self->lilvPlugins, plugin->uriObj);
	plugin->lilvPlugin = lilvPlugin;

//TODO free these when returning
// lilv_node_free(uriObj);

	if (!plugin) {
//TODO replace with log and disable?		
		printf("Plugin not found: %s\n",plugin->uri);
		return false;
	}

	bool supportsPatterns = lilv_plugin_has_feature(lilvPlugin,plugin->uriObj);
	printf("Supports patterns? %b\n",supportsPatterns);

	/* Create port structures */
	if (!createPorts(self,plugin)) 
		return false;

printf("Back from create_ports  n_params:%d\n",plugin->numParams);

//	if (self->n_audio_in == 0 || (in_fmt.channels != (int)self->n_audio_in && in_fmt.channels != 1)) {
//    	printf("Unable to map inputs to ports\n");
//		exit(1);
//	}

	/* Set control values */
	for (unsigned i = 0; i < plugin->numParams; ++i) {
		const Param* param = &plugin->params[i];
		LilvNode* sym   = lilv_new_string(self->world, param->sym);
		const LilvPort* port  = lilv_plugin_get_port_by_symbol(lilvPlugin, sym);
		lilv_node_free(sym);
		if (!port) {
			printf("Unknown port\n");
			return false;
		}

		plugin->ports[lilv_port_get_index(lilvPlugin, port)].value = param->value;
	}

//  out_fmt.channels = self->n_audio_out;
//    free(self->ports);

	/* Instantiate plugin and connect ports */
	const uint32_t n_ports = lilv_plugin_get_num_ports(lilvPlugin);

printf("HERE B n_ports:%d\n",n_ports);

//  float* const   in_buf  = alloc_audio_buffer(self->n_audio_in);
//  float* const   out_buf = alloc_audio_buffer(self->n_audio_out);

//	self.instance = lilv_plugin_instantiate(self->plugin, in_fmt.samplerate, NULL);

	/*
	for (uint32_t p = 0, i = 0, o = 0; p < n_ports; ++p) {
printf("HERE C\n");
		if (self.ports[p].type == TYPE_CONTROL) 
{			
printf("HERE C1\n");  <== XXX currently dying here
			lilv_instance_connect_port(self.instance, p, &self.ports[p].value);
}
		else 
{		
printf("HERE C2\n");
			lilv_instance_connect_port(self.instance, p, NULL);
}
	}
printf("HERE D\n");
*/

  /* Ports are now connected to buffers in interleaved format, so we can run
     a single frame at a time and avoid having to interleave buffers to
     read/write from/to sndfile. */

//  lilv_instance_activate(self.instance);
//  lilv_instance_run(self.instance, 1);
//  lilv_instance_deactivate(self.instance);

//  free(out_buf);
//  free(in_buf);
//	return st ? st : cleanup(0, &self);
}

void addPlugins()
{
	int numPlugins = 1;
 	char* pluginUris[] = {
		"http://benbriedis.com/lv2/luvie"
	};

	/*
typedef struct {
	char* uri;
	LilvNode* uriObj;
	LilvPlugin* plugin;
	LilvInstance* instance;
	unsigned numParams;
	Param* params;
	unsigned numPorts;
	Port* ports;
} Plugin;
*/

	/* Create world and plugin URI */
	App self = {NULL, numPlugins, NULL, NULL};

	self.world = lilv_world_new();
	lilv_world_load_all(self.world);
	self.lilvPlugins = lilv_world_get_all_plugins(self.world);

printf("addPlugins() - 2\n");	
/*	
	LilvWorld* world;
	int numPlugins;
	Plugin* plugins;
	LilvPlugins** lilvPlugins;
*/	
	//TODO check alloc OK + free
	self.plugins = calloc(sizeof(Plugin),numPlugins);

	bool ok = true;

	for (int i=0; i<numPlugins && ok; i++) {

		self.plugins[i].uri = pluginUris[i];

printf("addPlugins() - 2A\n");	
printf("addPlugins() - 2B %ld\n",(long)&self.plugins[i]);	
printf("addPlugins() - 2BB %ld\n",(long)&self);	
printf("addPlugins() - 2C\n");	
//		ok = ok && addPlugin(&self,&self.plugins[i]);
		ok = ok && addPlugin(&self,&self.plugins[i]);
printf("addPlugins() - 2D\n");	
	}

printf("addPlugins() - 3\n");	
	return ok;

//TODO if !ok disable the plugin I uess
}

