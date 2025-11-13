#include "pluginLoader.h"
#include "songEditor.h"
#include "lilv/lilv.h"
#include <assert.h>
#include <lv2/core/lv2.h>
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

static bool addPlugin(Self* self,Plugins* plugins,Plugin* plugin)
{
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
	LilvNode* featureUriObj = lilv_new_uri(plugins->world,"https://github.com/benbriedis/luvie#loops");
	bool supportsLoops = lilv_plugin_has_feature(lilvPlugin,featureUriObj);
	lilv_node_free(featureUriObj); 

	printf("Supports loops? %b\n",supportsLoops);

	/* Create port structures */
	if (!findControlPort(plugins,plugin)) 
		return false;

	/* Instantiate plugin and connect the control port */

printf("CALLING songEditor  addPlugin() - 2 lilvPlugin: %ld\n",(long)plugin->lilvPlugin);
printf("CALLING songEditor  addPlugin() - 2 sampleRate: %d\n",plugins->sampleRate);
//XXX maybe sample rate should be stored as a double?	


	//XXX hack - just passing our features through to our children for the moment
//	LV2_Feature* feature1 = {"http://lv2plug.in/ns/ext/urid#map",map}
//	LilvNode* features[] = {feature1};

	plugin->instance = lilv_plugin_instantiate(plugin->lilvPlugin, (double)plugins->sampleRate, self->features);
	assert(plugin->instance != NULL);

//	lilv_node_free(feature1); 

printf("CALLING songEditor  addPlugin() - 2 instance: %ld\n",(long)plugin->instance);
printf("CALLING songEditor  addPlugin() - 2 index: %ld\n",(long)plugin->controlPort->index);
printf("CALLING songEditor  addPlugin() - 2 message: %ld\n",(long)plugin->message);

//XXX MAYBE MEANT TO BE CALLED IN run() - see LV2 documentaton about threading, but conversely see...
//https://drobilla.net/docs/lilv/index.html#instances
//   MAYBE the host can choose? Looks like it needs to be called before activate though.



//XXX 'message' is used later when sending messages again which is a bit suspicious...


	lilv_instance_connect_port(plugin->instance,plugin->controlPort->index,plugin->message);
//XXX disconnect sometime?

printf("CALLING songEditor  addPlugin() - END\n");
}

bool instantiatePlugins(Self* self,Plugins* plugins)
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
//		plugin->message = calloc(sizeof(LoopMessage),1);  //TODO free
		plugin->message = calloc(100,1);  //TODO free
printf("Allocated memory to message: %ld\n",(long)plugin->message);

		ok = ok && addPlugin(self,plugins,plugin);
	}

	return ok;

//TODO if !ok disable the plugin I use
}


//TODO App => Plugins  or merge with song editor app

void activatePlugins(Plugins* plugins)
{
printf("CALLING songEditor activatePlugins() - START\n");			

	for (int i=0; i<plugins->numPlugins; i++) 
{		
printf("CALLING songEditor AAA i=%d\n",i);	
printf("CALLING songEditor AAA  instance: %ld\n",(long)plugins->plugins[i].instance);	

		lilv_instance_activate(plugins->plugins[i].instance);
printf("CALLING songEditor BBB i=%d\n",i);			
}

printf("CALLING songEditor activatePlugins() - END\n");			
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

