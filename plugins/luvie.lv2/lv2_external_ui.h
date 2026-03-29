/* lv2_external_ui.h — kxstudio External UI extension (bundled copy)
   URI: http://kxstudio.sf.net/ns/lv2ext/external-ui
   Widely supported by Ardour, Carla, etc.  Works on X11 and Wayland alike
   because the plugin opens its own top-level window (no embedding). */

#ifndef LV2_EXTERNAL_UI_H
#define LV2_EXTERNAL_UI_H

#include <lv2/ui/ui.h>

#define LV2_EXTERNAL_UI_URI      "http://kxstudio.sf.net/ns/lv2ext/external-ui"
#define LV2_EXTERNAL_UI_WIDGET   LV2_EXTERNAL_UI_URI "#Widget"
#define LV2_EXTERNAL_UI_HOST_URI LV2_EXTERNAL_UI_URI "#Host"

/* Deprecated alias still accepted by some hosts */
#define LV2_EXTERNAL_UI_DEPRECATED_URI "http://lv2plug.in/ns/extensions/ui#external"

#ifdef __cplusplus
extern "C" {
#endif

/* The widget struct IS the LV2UI_Handle: instantiate() returns a pointer to
   one of these.  The host casts the handle to this type and calls run/show/hide. */
typedef struct LV2_External_UI_Widget {
    void (*run) (struct LV2_External_UI_Widget*);   /* called periodically by host */
    void (*show)(struct LV2_External_UI_Widget*);   /* host wants UI visible */
    void (*hide)(struct LV2_External_UI_Widget*);   /* host wants UI hidden  */
} LV2_External_UI_Widget;

/* Passed to the UI via the LV2_EXTERNAL_UI_HOST_URI feature. */
typedef struct {
    void       (*ui_closed)(LV2UI_Controller controller); /* UI notifies host it closed */
    const char  *plugin_human_id;                         /* display name */
} LV2_External_UI_Host;

#ifdef __cplusplus
}
#endif

#endif /* LV2_EXTERNAL_UI_H */
