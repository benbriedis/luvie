// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

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
// Windows-specific resize support
// ---------------------------------------------------------------------------
#ifdef _WIN32
// <windows.h> arrives via FL/platform.H -> FL/win32.H.

// DIR_* -> the WMSZ_* code WM_SYSCOMMAND expects. Same idea as the X11 and
// Wayland maps above: the numbering is the platform's, not ours.
static const int wmsz_edge_map[] = {
    WMSZ_TOPLEFT,      // DIR_TL = 0
    WMSZ_TOP,          // DIR_T  = 1
    WMSZ_TOPRIGHT,     // DIR_TR = 2
    WMSZ_RIGHT,        // DIR_R  = 3
    WMSZ_BOTTOMRIGHT,  // DIR_BR = 4
    WMSZ_BOTTOM,       // DIR_B  = 5
    WMSZ_BOTTOMLEFT,   // DIR_BL = 6
    WMSZ_LEFT,         // DIR_L  = 7
};
#endif

// ---------------------------------------------------------------------------

bool AppWindow::wmResizeAvailable()
{
#if defined(FLTK_USE_WAYLAND) || defined(FLTK_USE_X11) || defined(_WIN32)
    return true;
#else
    // macOS. Cocoa already resizes a window from any of its edges, including a
    // few pixels inside the frame, so there is nothing here to add. Saying so
    // explicitly matters: the edge zone claims FL_MOVE to show a resize cursor
    // and then swallows the FL_PUSH, so offering it without a working handoff
    // would leave a 6px strip that looks resizable, isn't, and additionally
    // blocks whatever widget sits underneath it.
    return false;
#endif
}

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

bool AppWindow::startWmResize(int dir)
{
    // Every branch below is the same move: stop owning the pointer, then ask the
    // window manager to run its own interactive resize from here. Doing the
    // resize by hand from FL_DRAG events would work, but it would miss the
    // compositor's snapping, edge tiling and live constraints.
#ifdef FLTK_USE_WAYLAND
    if (!fl_display) {
        // Wayland backend — delegate to libdecor (bundled in libfltk.a)
        auto* win = reinterpret_cast<wld_window_fl*>(fl_wl_xid(this));
        if (!win || win->kind != 0 /*DECORATED*/ || !win->frame) return false;
        auto* scr = reinterpret_cast<Fl_Wayland_Screen_Driver*>(Fl::screen_driver());
        libdecor_frame_resize(win->frame, scr->get_wl_seat(),
                                 scr->get_serial(), wld_edge_map[dir]);
        Fl::pushed(nullptr);
        return true;
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
    return true;
#endif

#ifdef _WIN32
    HWND hwnd = fl_win32_xid(this);
    if (!hwnd) return false;
    // FLTK captured the mouse when it delivered FL_PUSH. The size loop below
    // reads the mouse itself, so it gets nothing until the capture is dropped —
    // this is the counterpart of XUngrabPointer on the X11 path.
    ReleaseCapture();
    // SendMessage, not PostMessage: WM_SYSCOMMAND/SC_SIZE runs a *modal* size
    // loop that does not return until the user lets go. It pumps messages
    // internally, so the window keeps repainting while it is being dragged.
    SendMessage(hwnd, WM_SYSCOMMAND, SC_SIZE + wmsz_edge_map[dir], 0);
    // The loop consumed the button release, so FLTK never sees FL_RELEASE and
    // would otherwise treat the next motion as a continuing drag.
    Fl::pushed(nullptr);
    return true;
#endif

    (void)dir;
    return false;
}

int AppWindow::handle(int event)
{
    // Edge-zone cursor and drag initiation. The zone lies *inside* the client
    // area, which is what makes it worth having: the window has ordinary native
    // decorations, but many window managers draw no side or bottom frame at all
    // (Luvie's own X11 session reports frame extents of 0,0,37,0 — a title bar
    // and nothing else), leaving no border to grab. Skipped entirely where the
    // platform resizes from its own edges; see wmResizeAvailable().
    if (wmResizeAvailable() &&
        (event == FL_MOVE || event == FL_ENTER || event == FL_PUSH)) {
        int dir = detectEdge();
        if (dir >= 0) {
            cursor(edgeCursor(dir));
            inEdgeZone = true;
            if (event == FL_PUSH && Fl::event_button() == FL_LEFT_MOUSE) {
                // Only claim the click if the handoff actually happened. If it
                // didn't, fall through so the widget under the pointer still
                // gets it rather than the click vanishing.
                if (startWmResize(dir)) return 1;
                cursor(FL_CURSOR_DEFAULT);
                inEdgeZone = false;
            } else {
                return 1;   // consume MOVE/ENTER so children don't steal cursor
            }
        } else if (inEdgeZone) {
            cursor(FL_CURSOR_DEFAULT);
            inEdgeZone = false;
        }
    }

    // FL_COMMAND is ctrl everywhere but macOS, where it is the Command key.
    // Accept both cases of the key: whether shift folds it is platform-dependent.
    if ((event == FL_KEYBOARD || event == FL_SHORTCUT) && (Fl::event_state() & FL_COMMAND) &&
        (Fl::event_key() == 'z' || Fl::event_key() == 'Z')) {
        if (Fl::event_state() & FL_SHIFT) { if (onRedo) onRedo(); }
        else                              { if (onUndo) onUndo(); }
        return 1;
    }

    // Same key routing as undo: a focused text input sees it first and keeps its
    // own select-all.
    if ((event == FL_KEYBOARD || event == FL_SHORTCUT) && (Fl::event_state() & FL_COMMAND) &&
        (Fl::event_key() == 'a' || Fl::event_key() == 'A')) {
        if (onSelectAll) onSelectAll();
        return 1;
    }

    // Not consumed when nothing is selected: falling through lets FLTK carry on
    // to the shortcut broadcast, which is how the hovered grid gets its chance
    // to delete the note under the cursor.
    if ((event == FL_KEYBOARD || event == FL_SHORTCUT) &&
        (Fl::event_key() == FL_Delete || Fl::event_key() == FL_BackSpace)) {
        if (onDeleteSelection && onDeleteSelection()) return 1;
    }

    if (event == FL_KEYBOARD && Fl::event_key() == FL_Escape) {
        if (onEscape) onEscape();
        return 1;
    }

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

        bool anyVisible = false;
        for (auto* p : popups)
            if (p->visible()) { anyVisible = true; break; }
        if (anyVisible) {
            int ex = Fl::event_x(), ey = Fl::event_y();
            bool inAny = false;
            for (auto* p : popups) {
                if (!p->visible()) continue;
                if (ex >= p->x() && ex < p->x() + p->w()
                 && ey >= p->y() && ey < p->y() + p->h()) {
                    inAny = true;
                    break;
                }
            }
            if (!inAny) {
                if (event == FL_PUSH) {
                    for (auto* p : popups) p->hide();
                    // A right-click outside the popup should dismiss it AND
                    // open the context menu of whatever was clicked, in a
                    // single click. Let the push propagate to that widget so
                    // its handler can open its own popup; don't swallow it.
                    if (Fl::event_button() == FL_RIGHT_MOUSE)
                        break;
                    closingClick = true;
                }
                return 1;
            }
        }
        break;
    }
    default:
        break;
    }

    // Past the popup handling, so the click that dismisses a popup is spent on
    // that alone.
    if (event == FL_PUSH && onClick)
        onClick(Fl::event_x(), Fl::event_y());

    return Fl_Double_Window::handle(event);
}
