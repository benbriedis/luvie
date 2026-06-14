#pragma once
#include "itimelineobserver.hpp"
#include "sessionManager.hpp"

// Observes the song and forwards every edit to SessionManager::markDirty().
// Loading a project also fires onTimelineChanged() (loadTimeline -> notify), so
// wrap loads in setSuppressed(true)/(false) to avoid a spurious dirty/is_dirty.
//
// Runs only on the FLTK UI thread (all mutations do); never touches the RT path.
class DirtyTracker : public ITimelineObserver {
public:
    explicit DirtyTracker(SessionManager* session) : session(session) {}

    void onTimelineChanged() override { if (!suppressed && session) session->markDirty(); }

    void setSuppressed(bool s) { suppressed = s; }

private:
    SessionManager* session;
    bool suppressed = false;
};
