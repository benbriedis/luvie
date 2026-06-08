// jackShim.cpp — dlopen-based shim for libjack.
//
// Luvie uses JACK for its real-time clock and MIDI output, but JACK is optional:
// the app already falls back to SimpleTransport + Native/Debug MIDI when no JACK
// server is running (see JackObserver). The only thing that *forced* libjack to be
// installed was the load-time NEEDED dependency the linker bakes in when you link
// `-ljack`. This file removes that: instead of linking the library, we compile
// against the JACK *headers* only and provide our own definitions of the handful
// of jack_* entry points Luvie calls. Each one forwards to the real symbol, which
// we resolve at runtime via dlopen("libjack.so.0").
//
// Result: a single portable binary. If libjack is present it is used; if it is
// absent, jack_client_open() returns null — which the existing code already treats
// as "server not running" — so the app degrades gracefully with no extra logic.
//
// We are NOT linking libjack, so defining symbols with its names creates no
// conflict. This is a clean-room equivalent of the GPL'd weakjack library.
//
// Note on threading: every entry point ensures the library is loaded on first use.
// The RT-thread entry points (jack_port_get_buffer / jack_midi_*) only ever run
// after a successful jack_client_open() on the UI thread, so by the time they run
// the handle is already loaded and ensureLoaded() is a single branch — no dlopen,
// no lock on the RT path.

#include <jack/jack.h>
#include <jack/midiport.h>
#include <dlfcn.h>

namespace {

// Real libjack entry points, resolved once via dlsym. decltype(&jack_foo) pulls the
// exact signature from the headers so the pointer types can never drift.
struct JackFns {
    decltype(&jack_client_open)          client_open          = nullptr;
    decltype(&jack_client_close)         client_close         = nullptr;
    decltype(&jack_activate)             activate             = nullptr;
    decltype(&jack_deactivate)           deactivate           = nullptr;
    decltype(&jack_get_sample_rate)      get_sample_rate      = nullptr;
    decltype(&jack_on_shutdown)          on_shutdown          = nullptr;
    decltype(&jack_set_process_callback) set_process_callback = nullptr;
    decltype(&jack_set_error_function)   set_error_function   = nullptr;
    decltype(&jack_set_info_function)    set_info_function    = nullptr;
    decltype(&jack_port_register)        port_register        = nullptr;
    decltype(&jack_port_unregister)      port_unregister      = nullptr;
    decltype(&jack_port_rename)          port_rename          = nullptr;
    decltype(&jack_port_get_buffer)      port_get_buffer      = nullptr;
    decltype(&jack_transport_start)      transport_start      = nullptr;
    decltype(&jack_transport_stop)       transport_stop       = nullptr;
    decltype(&jack_transport_locate)     transport_locate     = nullptr;
    decltype(&jack_transport_query)      transport_query      = nullptr;
    decltype(&jack_midi_clear_buffer)    midi_clear_buffer    = nullptr;
    decltype(&jack_midi_event_write)     midi_event_write     = nullptr;
};

JackFns g;
void*   handle = nullptr;
bool    tried  = false;

// Idempotent: dlopen libjack once and resolve every symbol we use. Returns true if
// libjack is available. After the first call this is just `return handle != nullptr`.
bool ensureLoaded() {
    if (tried) return handle != nullptr;
    tried = true;

    handle = dlopen("libjack.so.0", RTLD_NOW | RTLD_LOCAL);
    if (!handle) handle = dlopen("libjack.so", RTLD_NOW | RTLD_LOCAL);
    if (!handle) return false;

    #define LV_LOAD(field, sym) \
        g.field = reinterpret_cast<decltype(g.field)>(dlsym(handle, #sym))
    LV_LOAD(client_open,          jack_client_open);
    LV_LOAD(client_close,         jack_client_close);
    LV_LOAD(activate,             jack_activate);
    LV_LOAD(deactivate,           jack_deactivate);
    LV_LOAD(get_sample_rate,      jack_get_sample_rate);
    LV_LOAD(on_shutdown,          jack_on_shutdown);
    LV_LOAD(set_process_callback, jack_set_process_callback);
    LV_LOAD(set_error_function,   jack_set_error_function);
    LV_LOAD(set_info_function,    jack_set_info_function);
    LV_LOAD(port_register,        jack_port_register);
    LV_LOAD(port_unregister,      jack_port_unregister);
    LV_LOAD(port_rename,          jack_port_rename);
    LV_LOAD(port_get_buffer,      jack_port_get_buffer);
    LV_LOAD(transport_start,      jack_transport_start);
    LV_LOAD(transport_stop,       jack_transport_stop);
    LV_LOAD(transport_locate,     jack_transport_locate);
    LV_LOAD(transport_query,      jack_transport_query);
    LV_LOAD(midi_clear_buffer,    jack_midi_clear_buffer);
    LV_LOAD(midi_event_write,     jack_midi_event_write);
    #undef LV_LOAD
    return true;
}

} // namespace

// ── Forwarders ──────────────────────────────────────────────────────────────────
// Names and signatures match <jack/jack.h>. With libjack absent only client_open /
// set_error_function / set_info_function can actually be reached (the rest require a
// live client, which you can't get without libjack), but all guard defensively.

extern "C" {

jack_client_t* jack_client_open(const char* client_name, jack_options_t options,
                                jack_status_t* status, ...) {
    if (!ensureLoaded() || !g.client_open) {
        if (status) *status = JackFailure;
        return nullptr;
    }
    // We never pass the optional server-name vararg, so forwarding the fixed args is
    // sufficient (jack_client_open is variadic; extra args are genuinely optional).
    return g.client_open(client_name, options, status);
}

int jack_client_close(jack_client_t* client) {
    if (!ensureLoaded() || !g.client_close) return 0;
    return g.client_close(client);
}

int jack_activate(jack_client_t* client) {
    if (!ensureLoaded() || !g.activate) return -1;
    return g.activate(client);
}

int jack_deactivate(jack_client_t* client) {
    if (!ensureLoaded() || !g.deactivate) return -1;
    return g.deactivate(client);
}

jack_nframes_t jack_get_sample_rate(jack_client_t* client) {
    if (!ensureLoaded() || !g.get_sample_rate) return 0;
    return g.get_sample_rate(client);
}

void jack_on_shutdown(jack_client_t* client, JackShutdownCallback cb, void* arg) {
    if (!ensureLoaded() || !g.on_shutdown) return;
    g.on_shutdown(client, cb, arg);
}

int jack_set_process_callback(jack_client_t* client, JackProcessCallback cb, void* arg) {
    if (!ensureLoaded() || !g.set_process_callback) return -1;
    return g.set_process_callback(client, cb, arg);
}

void jack_set_error_function(void (*func)(const char*)) {
    if (!ensureLoaded() || !g.set_error_function) return;
    g.set_error_function(func);
}

void jack_set_info_function(void (*func)(const char*)) {
    if (!ensureLoaded() || !g.set_info_function) return;
    g.set_info_function(func);
}

jack_port_t* jack_port_register(jack_client_t* client, const char* port_name,
                                const char* port_type, unsigned long flags,
                                unsigned long buffer_size) {
    if (!ensureLoaded() || !g.port_register) return nullptr;
    return g.port_register(client, port_name, port_type, flags, buffer_size);
}

int jack_port_unregister(jack_client_t* client, jack_port_t* port) {
    if (!ensureLoaded() || !g.port_unregister) return -1;
    return g.port_unregister(client, port);
}

int jack_port_rename(jack_client_t* client, jack_port_t* port, const char* port_name) {
    if (!ensureLoaded() || !g.port_rename) return -1;
    return g.port_rename(client, port, port_name);
}

void* jack_port_get_buffer(jack_port_t* port, jack_nframes_t nframes) {
    if (!ensureLoaded() || !g.port_get_buffer) return nullptr;
    return g.port_get_buffer(port, nframes);
}

void jack_transport_start(jack_client_t* client) {
    if (!ensureLoaded() || !g.transport_start) return;
    g.transport_start(client);
}

void jack_transport_stop(jack_client_t* client) {
    if (!ensureLoaded() || !g.transport_stop) return;
    g.transport_stop(client);
}

int jack_transport_locate(jack_client_t* client, jack_nframes_t frame) {
    if (!ensureLoaded() || !g.transport_locate) return -1;
    return g.transport_locate(client, frame);
}

jack_transport_state_t jack_transport_query(const jack_client_t* client,
                                            jack_position_t* pos) {
    if (!ensureLoaded() || !g.transport_query) return JackTransportStopped;
    return g.transport_query(client, pos);
}

void jack_midi_clear_buffer(void* port_buffer) {
    if (!ensureLoaded() || !g.midi_clear_buffer) return;
    g.midi_clear_buffer(port_buffer);
}

int jack_midi_event_write(void* port_buffer, jack_nframes_t time,
                          const jack_midi_data_t* data, size_t data_size) {
    if (!ensureLoaded() || !g.midi_event_write) return -1;
    return g.midi_event_write(port_buffer, time, data, data_size);
}

} // extern "C"
