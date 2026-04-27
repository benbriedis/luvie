#include "appWindow.hpp"
#include <FL/Fl.H>
#include <FL/platform.H>     // fl_display (X11), fl_xid(), fl_wl_xid(), fl_wl_display()
#include <cstddef>           // offsetof

// _NET_WM_MOVERESIZE direction constants (also used as our edge IDs)
static constexpr int DIR_TL=0, DIR_T=1, DIR_TR=2, DIR_R=3;
static constexpr int DIR_BR=4, DIR_B=5, DIR_BL=6, DIR_L=7;

// ---------------------------------------------------------------------------
// Wayland-specific resize support
// We access FLTK internals via carefully-matched struct layout and
// a forward declaration of the non-virtual methods we need.
// ---------------------------------------------------------------------------
#ifdef FLTK_USE_WAYLAND
#include <wayland-client.h>

// libdecor resize-edge enum (matches the libdecor API, bundled or system)
enum wld_resize_edge {
    WLD_RESIZE_EDGE_NONE         = 0,
    WLD_RESIZE_EDGE_TOP          = 1,
    WLD_RESIZE_EDGE_BOTTOM       = 2,
    WLD_RESIZE_EDGE_LEFT         = 3,
    WLD_RESIZE_EDGE_TOP_LEFT     = 4,
    WLD_RESIZE_EDGE_BOTTOM_LEFT  = 5,
    WLD_RESIZE_EDGE_RIGHT        = 6,
    WLD_RESIZE_EDGE_TOP_RIGHT    = 7,
    WLD_RESIZE_EDGE_BOTTOM_RIGHT = 8,
};

struct libdecor_frame;  // opaque; we only hold a pointer

// System libdecor (new FLTK 1.5 builds use system libdecor directly).
extern "C" void libdecor_frame_resize(
    struct libdecor_frame*, struct wl_seat*, uint32_t, int edge);

// Minimal mirror of FLTK's internal struct wld_window (Fl_Wayland_Window_Driver.H).
// Field offsets on 64-bit (all pointers = 8 bytes, wl_list = 2 pointers = 16 bytes):
//   fl_win(8) + outputs/wl_list(16) + wl_surface(8) + frame_cb(8) +
//   buffer(8) + xdg_surface(8) + union/frame(8) + custom_cursor(8) + kind(4)
struct wld_window_fl {
    void           *fl_win;         //  0
    void           *outputs_prev;   //  8  } wl_list outputs
    void           *outputs_next;   // 16  }
    void           *wl_surface;     // 24
    void           *frame_cb;       // 32
    void           *buffer;         // 40
    void           *xdg_surface;    // 48
    libdecor_frame *frame;          // 56  (union first member, valid when kind==DECORATED)
    void           *custom_cursor;  // 64
    int             kind;           // 72  (0 == DECORATED)
};
static_assert(offsetof(wld_window_fl, frame) == 56, "wld_window layout mismatch");
static_assert(offsetof(wld_window_fl, kind)  == 72, "wld_window layout mismatch");

// Forward declaration of the two non-virtual Fl_Wayland_Screen_Driver methods
// we need. Non-virtual calls resolve directly to the mangled symbol in libfltk.a;
// no vtable lookup involved. Fl_Wayland_Screen_Driver uses single inheritance so
// the pointer from Fl::screen_driver() requires no offset adjustment.
class Fl_Wayland_Screen_Driver {
public:
    uint32_t        get_serial();
    struct wl_seat *get_wl_seat();
};

// Direction-to-resize-edge mapping (our DIR_* → wld_resize_edge)
static const int wld_edge_map[] = {
    WLD_RESIZE_EDGE_TOP_LEFT,     // DIR_TL = 0
    WLD_RESIZE_EDGE_TOP,          // DIR_T  = 1
    WLD_RESIZE_EDGE_TOP_RIGHT,    // DIR_TR = 2
    WLD_RESIZE_EDGE_RIGHT,        // DIR_R  = 3
    WLD_RESIZE_EDGE_BOTTOM_RIGHT, // DIR_BR = 4
    WLD_RESIZE_EDGE_BOTTOM,       // DIR_B  = 5
    WLD_RESIZE_EDGE_BOTTOM_LEFT,  // DIR_BL = 6
    WLD_RESIZE_EDGE_LEFT,         // DIR_L  = 7
};
#endif // FLTK_USE_WAYLAND

// ---------------------------------------------------------------------------
// X11-specific resize support
// ---------------------------------------------------------------------------
#ifdef FLTK_USE_X11
#include <X11/Xatom.h>
#endif

// ---------------------------------------------------------------------------

int AppWindow::detectEdge() const
{
    int ex = Fl::event_x(), ey = Fl::event_y();
    bool l = ex < edgeZone,      r = ex >= w() - edgeZone;
    bool t = ey < edgeZone,      b = ey >= h() - edgeZone;
    if (!l && !r && !t && !b) return -1;
    if (t && l) return DIR_TL;
    if (t && r) return DIR_TR;
    if (b && r) return DIR_BR;
    if (b && l) return DIR_BL;
    if (t)      return DIR_T;
    if (r)      return DIR_R;
    if (b)      return DIR_B;
                return DIR_L;
}

Fl_Cursor AppWindow::edgeCursor(int dir) const
{
    switch (dir) {
        case DIR_TL: return FL_CURSOR_NW;
        case DIR_T:  return FL_CURSOR_N;
        case DIR_TR: return FL_CURSOR_NE;
        case DIR_R:  return FL_CURSOR_E;
        case DIR_BR: return FL_CURSOR_SE;
        case DIR_B:  return FL_CURSOR_S;
        case DIR_BL: return FL_CURSOR_SW;
        case DIR_L:  return FL_CURSOR_W;
        default:     return FL_CURSOR_DEFAULT;
    }
}

void AppWindow::startWmResize(int dir)
{
#ifdef FLTK_USE_WAYLAND
    if (!fl_display) {
        // Wayland backend — delegate to libdecor (bundled in libfltk.a)
        auto* win = reinterpret_cast<wld_window_fl*>(fl_wl_xid(this));
        if (!win || win->kind != 0 /*DECORATED*/ || !win->frame) return;
        auto* scr = reinterpret_cast<Fl_Wayland_Screen_Driver*>(Fl::screen_driver());
        libdecor_frame_resize(win->frame, scr->get_wl_seat(),
                                 scr->get_serial(), wld_edge_map[dir]);
        Fl::pushed(nullptr);
        return;
    }
#endif

#ifdef FLTK_USE_X11
    // X11 backend — release implicit pointer grab then send _NET_WM_MOVERESIZE
    XUngrabPointer(fl_display, CurrentTime);

    static Atom wmMoveResize =
        XInternAtom(fl_display, "_NET_WM_MOVERESIZE", False);

    XEvent ev = {};
    ev.xclient.type         = ClientMessage;
    ev.xclient.window       = fl_xid(this);
    ev.xclient.message_type = wmMoveResize;
    ev.xclient.format       = 32;
    ev.xclient.data.l[0]    = Fl::event_x_root();
    ev.xclient.data.l[1]    = Fl::event_y_root();
    ev.xclient.data.l[2]    = dir;
    ev.xclient.data.l[3]    = 1;   // button (left)
    ev.xclient.data.l[4]    = 1;   // source (normal app)
    XSendEvent(fl_display, DefaultRootWindow(fl_display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    XFlush(fl_display);
#endif
}

int AppWindow::handle(int event)
{
    // Edge-zone cursor and drag initiation.
    if (event == FL_MOVE || event == FL_ENTER || event == FL_PUSH) {
        int dir = detectEdge();
        if (dir >= 0) {
            cursor(edgeCursor(dir));
            inEdgeZone = true;
            if (event == FL_PUSH && Fl::event_button() == FL_LEFT_MOUSE) {
                startWmResize(dir);
                return 1;
            }
            return 1;   // consume MOVE/ENTER so children don't steal cursor
        }
        if (inEdgeZone) {
            cursor(FL_CURSOR_DEFAULT);
            inEdgeZone = false;
        }
    }

    if (event == FL_KEYBOARD && Fl::event_key() == FL_Escape)
        return 1;

    switch (event) {
    case FL_PUSH:
    case FL_DRAG:
    case FL_RELEASE:
    case FL_MOVE:
    case FL_MOUSEWHEEL: {
        // Swallow the click that closed a popup.
        if ((event == FL_RELEASE || event == FL_DRAG) && closingClick) {
            if (event == FL_RELEASE) closingClick = false;
            return 1;
        }

        Fl_Window* active = nullptr;
        for (auto* p : popups)
            if (p->visible()) { active = p; break; }
        if (active) {
            int ex = Fl::event_x(), ey = Fl::event_y();
            bool inPopup = ex >= active->x() && ex < active->x() + active->w()
                        && ey >= active->y() && ey < active->y() + active->h();
            if (!inPopup) {
                if (event == FL_PUSH) { active->hide(); closingClick = true; }
                return 1;
            }
        }
        break;
    }
    default:
        break;
    }
    return Fl_Double_Window::handle(event);
}
