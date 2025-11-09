#include "pluginLoader.h"
#include "lilv/lilv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static int findControlPort(Plugins* plugins,Plugin* plugin)
{
	const LilvPlugin* lilvPlugin = plugin->lilvPlugin;
	const uint32_t numPorts = lilv_plugin_get_num_ports(lilvPlugin);

//TODO save the control port in its own special place...

	bool found = false;

	for (uint32_t i = 0; i < numPorts; i++) {
		const LilvPort* lilvPort = lilv_plugin_get_port_by_index(lilvPlugin, i);  //TODO free?

		LilvNode* uriObj = lilv_new_uri(plugins->world, "https://github.com/benbriedis/luvie#loopsControl");

		/* Look for our loop/pattern input control port: */
		if (lilv_port_supports_event(lilvPlugin,lilvPort,uriObj)) {
			/* Assuming we are an input port, etc */

			printf("FOUND CONTROL PORT on %u\n",i);

			Port* controlPort = calloc(sizeof(Port),1);   //TODO free
			plugin->controlPort = controlPort;
			controlPort->lilvPort = lilvPort;
			controlPort->index = i;
			found = true;
		}

		lilv_node_free(uriObj);
	}

	return found;
}

//XXX We probably also need to capture and midi-on and bundle it up in our midi output. In time 
//    maybe similar for audio and possibly control ports.

static bool addPlugin(Plugins* plugins,Plugin* plugin)
{
printf("addPlugin()  - A\n");

	/* Get plugin */
//XXX cf adding to plugin and freeing at the end ... (also used in ports)	
//TODO store uri in plugin too...

	LilvNode* uriObj = lilv_new_uri(plugins->world, plugin->uri);
	const LilvPlugin* lilvPlugin = lilv_plugins_get_by_uri(plugins->lilvPlugins, uriObj);
	plugin->lilvPlugin = lilvPlugin;
	lilv_node_free(uriObj); 

	if (!plugin) {
//TODO replace with log and disable?		
		printf("Plugin not found: %s\n",plugin->uri);
		return false;
	}

//TODO use at some point
	bool supportsLoops = lilv_plugin_has_feature(lilvPlugin,"https://github.com/benbriedis/luvie#loops");
	printf("Supports loops? %b\n",supportsLoops);

	/* Create port structures */
	if (!findControlPort(plugins,plugin)) 
		return false;

	/* Instantiate plugin and connect the control port */

	plugin->instance = lilv_plugin_instantiate(plugin->lilvPlugin, plugins->sampleRate, NULL);

//XXX MAYBE MEANT TO BE CALLED IN run() - see LV2 documentaton about threading, but conversely see...
//https://drobilla.net/docs/lilv/index.html#instances
//   MAYBE the host can choose? Looks like it needs to be called before activate though.

	lilv_instance_connect_port(plugin->instance,plugin->controlPort->index,plugin->message);
//XXX disconnect sometime?

}

bool instantiatePlugins(Plugins* plugins)
{
	plugins->numPlugins = 1;

 	char* pluginUris[] = {
		"https://github.com/benbriedis/luvie/harmony"
	};

	/* Create world and plugin URI */

	plugins->world = lilv_world_new();
	lilv_world_load_all(plugins->world);
	plugins->lilvPlugins = lilv_world_get_all_plugins(plugins->world);

	//TODO check alloc OK + free
	plugins->plugins = calloc(sizeof(Plugin),plugins->numPlugins);

	bool ok = true;

	for (int i=0; i<plugins->numPlugins && ok; i++) {
		Plugin* plugin = &plugins->plugins[i];
		plugin->uri = pluginUris[i];
//XXX Q should we try using 'Patch' messages?
//XXX or should this allow for multiple messages?
		plugin->message = calloc(sizeof(LoopMessage),1);  //TODO free

		ok = ok && addPlugin(plugins,plugin);
	}

	return ok;

//TODO if !ok disable the plugin I use
}

void runPlugins(Plugins* plugins)
{
//XXX or integrate into songEditor proper

	for (int i=0; i<plugins->numPlugins; i++) {
//run ***		
		lilv_instance_activate(plugins->plugins[i].instance);
//  lilv_instance_run(plugin->instance, 1);
	}
}


//TODO App => Plugins  or merge with song editor app

void activatePlugins(Plugins* plugins)
{
	for (int i=0; i<plugins->numPlugins; i++) 
		lilv_instance_activate(plugins->plugins[i].instance);
}

void deactivatePlugins(Plugins* plugins)
{
	for (int i=0; i<plugins->numPlugins; i++) 
		lilv_instance_deactivate(plugins->plugins[i].instance);
}

void cleanupPlugins(Plugins* plugins)
{
	for (int i=0; i<plugins->numPlugins; i++) {
		lilv_instance_free(plugins->plugins[i].instance);
	}

	lilv_world_free(plugins->world);
//	free(plugins->ports);
//	free(plugins->params);
}

