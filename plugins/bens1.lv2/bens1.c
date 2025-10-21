#include <lv2/core/lv2.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#define AMP_URI "http://benbriedis.com/lv2/bens1"

/*
   In code, ports are referred to by index.  An enumeration of port indices
   should be defined for readability.
*/
typedef enum { AMP_GAIN = 0, AMP_INPUT = 1, AMP_OUTPUT = 2 } PortIndex;

/*
   All data associated with a plugin instance is stored here.
   Port buffers.
*/
typedef struct {
	const float* gain;
	const float* input;
	float*       output;
} Self;

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
  Self* amp = (Self*)calloc(1, sizeof(Self));

  return (LV2_Handle)amp;
}

/*
   The plugin must store the data location, but data may not be accessed except in run().
   This method is in the 'audio' threading class, and is called in the same context as run().
*/
static void connect_port(LV2_Handle instance, uint32_t port, void* data)
{
	Self* amp = (Self*)instance;

	switch ((PortIndex)port) {
		case AMP_GAIN:
			amp->gain = (const float*)data;
			break;
		case AMP_INPUT:
			amp->input = (const float*)data;
			break;
		case AMP_OUTPUT:
			amp->output = (float*)data;
			break;
	}
}

/*
   The `activate()` method is called by the host to initialise and prepare the
   plugin instance for running.  The plugin must reset all internal state
   except for buffer locations set by `connect_port()`.  
   This plugin has no other internal state and does nothing.

   This method is in the 'instantiation' threading class. No other
   methods on this instance will be called concurrently with it.
*/
static void activate(LV2_Handle instance)
{}

#define DB_CO(g) ((g) > -90.0f ? powf(10.0f, (g) * 0.05f) : 0.0f)

/*
	The `run()` method is the main process function of the plugin.
	It processes a block of audio in the audio context.  
	Since this plugin is `lv2:hardRTCapable`, `run()` must be real-time safe, 
	so blocking (e.g. with a mutex) or memory allocation are not allowed.
*/
static void run(LV2_Handle instance, uint32_t n_samples)
{
	const Self* amp = (const Self*)instance;

	const float gain = *(amp->gain);
	const float* const input = amp->input;
	float* const output = amp->output;

	const float coef = DB_CO(gain);

	for (uint32_t pos = 0; pos < n_samples; pos++) 
		output[pos] = input[pos] * coef;
}

/*
   The host will not call `run()` again until another call to `activate()`.
   It is mainly useful for more advanced plugins with 'live' characteristics such as those with
   auxiliary processing threads.

   This method is in the ``instantiation'' threading class, so no other
   methods on this instance will be called concurrently with it.
*/
static void deactivate(LV2_Handle instance)
{}

/*
   Destroy a plugin instance (counterpart to `instantiate()`).

   This method is in the ``instantiation'' threading class, so no other
   methods on this instance will be called concurrently with it.
*/
static void cleanup(LV2_Handle instance)
{
	free(instance);
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
static const LV2_Descriptor descriptor = {AMP_URI, instantiate, connect_port, activate, 
	run, deactivate, cleanup, extension_data};

/*
   The `lv2_descriptor()` function is the entry point to the plugin library. 
   The host will load the library and call this function repeatedly with increasing
   indices to find all the plugins defined in the library.  The index is not an
   identify of the plugin.

   This method is in the 'discovery' threading class, so no other functions
   or methods in this plugin library will be called concurrently with it.
*/
LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
	return index == 0 ? &descriptor : NULL;
}
