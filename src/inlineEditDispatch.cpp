#include "inlineEditDispatch.hpp"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>

namespace {
    Fl_Widget*            activeHost       = nullptr;
    std::function<void()> commitFn;
    bool                  commitPending    = false;
    Fl_Event_Dispatch     originalDispatch = nullptr;

    int editDispatch(int event, Fl_Window* window)
    {
        if (commitPending) {
            if (event == FL_DRAG)    return 1;
            if (event == FL_RELEASE) {
                commitPending = false;
                if (commitFn) commitFn();
                return 1;
            }
        }
        if (activeHost && event == FL_PUSH) {
            int  ex      = Fl::event_x(), ey = Fl::event_y();
            bool inside  = ex >= activeHost->x() && ex < activeHost->x() + activeHost->w()
                        && ey >= activeHost->y() && ey < activeHost->y() + activeHost->h();
            if (!inside) {
                commitPending = true;
                return 1;
            }
        }
        return Fl::handle_(event, window);
    }
}

void InlineEditDispatch::install(Fl_Widget* host, std::function<void()> onCommit)
{
    activeHost       = host;
    commitFn         = std::move(onCommit);
    commitPending    = false;
    originalDispatch = Fl::event_dispatch();
    Fl::event_dispatch(editDispatch);
}

void InlineEditDispatch::uninstall()
{
    if (originalDispatch) {
        Fl::event_dispatch(originalDispatch);
        originalDispatch = nullptr;
    }
    activeHost    = nullptr;
    commitFn      = nullptr;
    commitPending = false;
}
