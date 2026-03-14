#ifndef INLINE_EDIT_DISPATCH_HPP
#define INLINE_EDIT_DISPATCH_HPP

#include <FL/Fl_Widget.H>
#include <functional>

// Shared global event interceptor for inline text editing.
// While active, any FL_PUSH outside the host widget's bounds will be consumed
// and the commit callback fired on the following FL_RELEASE.
namespace InlineEditDispatch {
    void install(Fl_Widget* host, std::function<void()> onCommit);
    void uninstall();
}

#endif
