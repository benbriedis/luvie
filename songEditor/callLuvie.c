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
	const char* sym;   ///< Port symbol
	float       value; ///< Control value
} Param;

/* Port type (only float ports are supported) */
typedef enum { TYPE_CONTROL } PortType;

/* Runtime port information */
typedef struct {
	const LilvPort* lilv_port; ///< Port description
	PortType        type;      ///< Datatype
	uint32_t        index;     ///< Port index
	float           value;     ///< Control value (if applicable)
	bool            is_input;  ///< True iff an input port
	bool            optional;  ///< True iff connection optional
} Port;

/* Application state */
typedef struct {
	LilvWorld*        world;
	const LilvPlugin* plugin;
	LilvInstance*     instance;
	unsigned          n_params;
	Param*            params;
	unsigned          n_ports;
	unsigned          n_audio_in;
	unsigned          n_audio_out;
	Port*             ports;
} LV2Apply;


/* Clean up all resources. */
static int cleanup(int status, LV2Apply* self)
{
	lilv_instance_free(self->instance);
	lilv_world_free(self->world);
	free(self->ports);
	free(self->params);
	return status;
}

/* Create port structures from data (via create_port()) for all ports. */
static int create_ports(LV2Apply* self)
{
	LilvWorld* world = self->world;
	const uint32_t n_ports = lilv_plugin_get_num_ports(self->plugin);

	self->n_ports = n_ports;
	self->ports   = (Port*)calloc(self->n_ports, sizeof(Port));

	/* Get default values for all ports */
	float* values = (float*)calloc(n_ports, sizeof(float));
	lilv_plugin_get_port_ranges_float(self->plugin, NULL, NULL, values);

	LilvNode* lv2_InputPort   = lilv_new_uri(world, LV2_CORE__InputPort);
	LilvNode* lv2_OutputPort  = lilv_new_uri(world, LV2_CORE__OutputPort);
	LilvNode* lv2_AudioPort   = lilv_new_uri(world, LV2_CORE__AudioPort);
	LilvNode* lv2_ControlPort = lilv_new_uri(world, LV2_CORE__ControlPort);
	LilvNode* lv2_connectionOptional = lilv_new_uri(world, LV2_CORE__connectionOptional);

	for (uint32_t i = 0; i < n_ports; ++i) {
		Port* port  = &self->ports[i];
		const LilvPort* lport = lilv_plugin_get_port_by_index(self->plugin, i);

		port->lilv_port = lport;
		port->index = i;
		port->value = isnan(values[i]) ? 0.0f : values[i];
		port->optional = lilv_port_has_property(self->plugin, lport, lv2_connectionOptional);

		/* Check if port is an input or output */
		if (lilv_port_is_a(self->plugin, lport, lv2_InputPort)) 
			port->is_input = true;
		else if (!lilv_port_is_a(self->plugin, lport, lv2_OutputPort) && !port->optional) {
			printf("Port %u is neither input nor output\n", i);  //XXX errors should go to stderr
			exit(1);
		}

printf("is_input: %d\n",port->is_input);		

LilvNode* symbol = lilv_port_get_symbol(self->plugin,lport);
printf("symbol: %s\n",lilv_node_as_string(symbol));

LilvNode* name = lilv_port_get_name(self->plugin,lport);
printf("name: %s\n",lilv_node_as_string(name));
lilv_node_free(name);

LilvNode* portSupportPatterns = lilv_new_uri(self->world, "https://github.com/benbriedis/luvie#patterns"); //XXX is prefix available?
bool supportsPatterns = lilv_port_supports_event(self->plugin,lport,portSupportPatterns);
printf("Port supports patterns? %b\n",supportsPatterns);
lilv_node_free(portSupportPatterns);

//XXX want our child to have a control input port so we can give it pattern on/off events
//    We probably also need to capture and midi-on and bundle it up in our midi output. In time 
//    maybe similar for audio and possibly control ports.

		/* Check if port is an audio or control port */
		if (lilv_port_is_a(self->plugin, lport, lv2_ControlPort)) {
			printf("Got control port on %u\n",i);
			port->type = TYPE_CONTROL;
		}
		else if (!port->optional) {
			printf("Port %u has unsupported type\n", i);
			exit(1);
		}
	}

	lilv_node_free(lv2_connectionOptional);
	lilv_node_free(lv2_ControlPort);
	lilv_node_free(lv2_AudioPort);
	lilv_node_free(lv2_OutputPort);
	lilv_node_free(lv2_InputPort);
	free(values);

	return 0;
}


//int main(int argc, char** argv)
void addPlugin()
{
 	char pluginUri[] = "http://benbriedis.com/lv2/luvie";

	LV2Apply self = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0, 0, 0, NULL};

	/* Create world and plugin URI */
	self.world = lilv_world_new();
	lilv_world_load_all(self.world);
	const LilvPlugins* plugins = lilv_world_get_all_plugins(self.world);

	LilvNode* uri = lilv_new_uri(self.world, pluginUri);

	/* Get plugin */
	const LilvPlugin* plugin = lilv_plugins_get_by_uri(plugins, uri);
	lilv_node_free(uri);
	if (!(self.plugin = plugin)) {
//TODO replace with log and disable?		
		printf("Plugin not found\n");
		exit(1);
	}

	LilvNode* patternsFeature = lilv_new_uri(self.world, "https://github.com/benbriedis/luvie#patterns"); //XXX is prefix available?
	bool supportsPatterns = lilv_plugin_has_feature(plugin,patternsFeature);
	printf("Supports patterns? %b\n",supportsPatterns);
	lilv_node_free(patternsFeature);


//TODO check capabilities... or maybe of the port

	/* Create port structures */
	if (create_ports(&self)) 
		return 5;

//	if (self.n_audio_in == 0 || (in_fmt.channels != (int)self.n_audio_in && in_fmt.channels != 1)) {
//    	printf("Unable to map inputs to ports\n");
//		exit(1);
//	}

	/* Set control values */
	for (unsigned i = 0; i < self.n_params; ++i) {
		const Param* param = &self.params[i];
		LilvNode* sym   = lilv_new_string(self.world, param->sym);
		const LilvPort* port  = lilv_plugin_get_port_by_symbol(plugin, sym);
		lilv_node_free(sym);
		if (!port) {
			printf("Unknown port\n");
			exit(1);
		}

		self.ports[lilv_port_get_index(plugin, port)].value = param->value;
	}

//  out_fmt.channels = self.n_audio_out;
//    free(self.ports);

	/* Instantiate plugin and connect ports */
	const uint32_t n_ports = lilv_plugin_get_num_ports(plugin);

//  float* const   in_buf  = alloc_audio_buffer(self.n_audio_in);
//  float* const   out_buf = alloc_audio_buffer(self.n_audio_out);

//	self.instance = lilv_plugin_instantiate(self.plugin, in_fmt.samplerate, NULL);

	for (uint32_t p = 0, i = 0, o = 0; p < n_ports; ++p) {
		if (self.ports[p].type == TYPE_CONTROL) 
			lilv_instance_connect_port(self.instance, p, &self.ports[p].value);
		else 
			lilv_instance_connect_port(self.instance, p, NULL);
	}

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

