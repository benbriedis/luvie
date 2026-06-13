#include "luvie_dsp.h"
#include <lv2/state/state.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LUVIE_STATE_URI "https://github.com/benbriedis/luvie#FullState"

/* -----------------------------------------------------------------------
   LV2 callbacks
   ----------------------------------------------------------------------- */

static void mapURIs(LV2_URID_Map* map, URIs* uris)
{
    uris->atom_Blank         = map->map(map->handle, LV2_ATOM__Blank);
    uris->atom_Object        = map->map(map->handle, LV2_ATOM__Object);
    uris->atom_Sequence      = map->map(map->handle, LV2_ATOM__Sequence);
    uris->atom_URID          = map->map(map->handle, LV2_ATOM__URID);
    uris->atom_Chunk         = map->map(map->handle, LV2_ATOM__Chunk);
    uris->time_Position      = map->map(map->handle, LV2_TIME__Position);
    uris->time_bar           = map->map(map->handle, LV2_TIME__bar);
    uris->time_barBeat       = map->map(map->handle, LV2_TIME__barBeat);
    uris->time_beatsPerBar   = map->map(map->handle, LV2_TIME__beatsPerBar);
    uris->time_beatsPerMinute= map->map(map->handle, LV2_TIME__beatsPerMinute);
    uris->time_speed         = map->map(map->handle, LV2_TIME__speed);
    uris->luvie_state        = map->map(map->handle, LUVIE_STATE_URI);
    uris->state_StateChanged = map->map(map->handle, LV2_STATE__StateChanged);
}

static LV2_Handle instantiate(
    const LV2_Descriptor* descriptor,
    double sampleRate,
    const char* bundlePath,
    const LV2_Feature* const* features)
{
    (void)descriptor; (void)bundlePath;

    Self* self = (Self*)calloc(1, sizeof(Self));
    if (!self)
        return NULL;

    const char* missing = lv2_features_query(
        features,
        LV2_LOG__log,  &self->logger.log, false,
        LV2_URID__map, &self->map,        true,
        NULL);

    lv2_log_logger_set_map(&self->logger, self->map);
    if (missing) {
        lv2_log_error(&self->logger, "Missing feature <%s>\n", missing);
        free(self);
        return NULL;
    }

    mapURIs(self->map, &self->uris);
    lv2_atom_forge_init(&self->forge, self->map);

    self->sampleRate  = sampleRate;
    self->speed       = 0.0f;
    self->beatsPerBar = 4.0f;
    self->bpm         = 120.0f;
    self->bar         = 0;
    self->barBeat     = 0.0f;

    /* timeline starts empty — will be populated by first UI message */

    return (LV2_Handle)self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data)
{
    Self* self = (Self*)instance;
    switch (port) {
        case PORT_CONTROL_IN: self->controlIn = (const LV2_Atom_Sequence*)data; break;
        case PORT_NOTIFY_OUT: self->notifyOut = (LV2_Atom_Sequence*)data;       break;
    }
}

static void activate(LV2_Handle instance)
{
    Self* self = (Self*)instance;
    self->speed   = 0.0f;
    self->bar     = 0;
    self->barBeat = 0.0f;
}

static void run(LV2_Handle instance, uint32_t sample_count)
{
    (void)sample_count;
    Self* self = (Self*)instance;
    const URIs* uris = &self->uris;

    /* Set up forge to write into notifyOut */
    const int hasNotify = (self->notifyOut != NULL);
    LV2_Atom_Forge_Frame notifyFrame;
    if (hasNotify) {
        const uint32_t notifyCapacity = self->notifyOut->atom.size;
        lv2_atom_sequence_clear(self->notifyOut);
        self->notifyOut->atom.type = uris->atom_Sequence;
        lv2_atom_forge_set_buffer(&self->forge,
                                  (uint8_t*)self->notifyOut,
                                  sizeof(LV2_Atom) + notifyCapacity);
        lv2_atom_forge_sequence_head(&self->forge, &notifyFrame, 0);
    }

    LV2_ATOM_SEQUENCE_FOREACH(self->controlIn, ev) {
        /* Store full JSON state blobs for LV2 state saving */
        if (ev->body.type == uris->luvie_state) {
            uint32_t size = ev->body.size;
            free(self->stateJson);
            self->stateJson = (char*)malloc(size);
            if (self->stateJson) {
                memcpy(self->stateJson, LV2_ATOM_BODY(&ev->body), size);
                self->stateJsonSize = size;
            }
            /* The plugin's internal state changed; tell the host to mark
               its project dirty (and re-save us) via state:StateChanged. */
            self->stateChanged = true;
            continue;
        }

        /* Forward time:Position atoms to the UI */
        if (ev->body.type == uris->atom_Object || ev->body.type == uris->atom_Blank) {
            const LV2_Atom_Object* obj = (const LV2_Atom_Object*)&ev->body;
            if (obj->body.otype == uris->time_Position && hasNotify) {
                lv2_atom_forge_frame_time(&self->forge, ev->time.frames);
                lv2_atom_forge_write(&self->forge, &ev->body,
                                     sizeof(LV2_Atom) + ev->body.size);
            }
        }
    }

    /* Emit a state:StateChanged notification once per pending UI edit so the
       host marks its project dirty. It carries no body — the type is the signal. */
    if (hasNotify && self->stateChanged) {
        LV2_Atom_Forge_Frame objFrame;
        lv2_atom_forge_frame_time(&self->forge, 0);
        lv2_atom_forge_object(&self->forge, &objFrame, 0, self->uris.state_StateChanged);
        lv2_atom_forge_pop(&self->forge, &objFrame);
        self->stateChanged = false;
    }

    if (hasNotify)
        lv2_atom_forge_pop(&self->forge, &notifyFrame);
}

static void deactivate(LV2_Handle instance) { (void)instance; }

static void cleanup(LV2_Handle instance)
{
    Self* self = (Self*)instance;
    free(self->stateJson);
    free(self);
}

static LV2_State_Status dsp_save(
    LV2_Handle                 instance,
    LV2_State_Store_Function   store,
    LV2_State_Handle           handle,
    uint32_t                   flags,
    const LV2_Feature* const*  features)
{
    (void)flags; (void)features;
    Self* self = (Self*)instance;
    if (!self->stateJson || self->stateJsonSize == 0)
        return LV2_STATE_SUCCESS;

    return store(handle,
                 self->uris.luvie_state,
                 self->stateJson,
                 self->stateJsonSize,
                 self->uris.atom_Chunk,
                 LV2_STATE_IS_POD);
}

static LV2_State_Status dsp_restore(
    LV2_Handle                    instance,
    LV2_State_Retrieve_Function   retrieve,
    LV2_State_Handle              handle,
    uint32_t                      flags,
    const LV2_Feature* const*     features)
{
    (void)flags; (void)features;
    Self* self = (Self*)instance;

    size_t   size     = 0;
    uint32_t type     = 0;
    uint32_t valflags = 0;
    const void* data = retrieve(handle, self->uris.luvie_state, &size, &type, &valflags);
    if (!data || type != self->uris.atom_Chunk || size == 0)
        return LV2_STATE_SUCCESS;

    free(self->stateJson);
    self->stateJson = (char*)malloc(size);
    if (!self->stateJson)
        return LV2_STATE_ERR_NO_SPACE;
    memcpy(self->stateJson, data, size);
    self->stateJsonSize = (uint32_t)size;

    /* Write JSON to a file so the UI can read it when it next opens */
    char path[256];
    snprintf(path, sizeof(path), "/tmp/luvie_state_%d.json", (int)getpid());
    FILE* f = fopen(path, "wb");
    if (f) {
        /* stateJsonSize includes null terminator; write without it */
        fwrite(data, 1, size > 0 ? size - 1 : 0, f);
        fclose(f);
    }

    return LV2_STATE_SUCCESS;
}

static const void* extension_data(const char* uri)
{
    static const LV2_State_Interface state_iface = { dsp_save, dsp_restore };
    if (!strcmp(uri, LV2_STATE__interface))
        return &state_iface;
    return NULL;
}

static const LV2_Descriptor descriptor = {
    LUVIE_DSP_URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
    return index == 0 ? &descriptor : NULL;
}
