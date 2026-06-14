#pragma once
#include <functional>
#include "timelineIO.hpp"

// Unifies the three ways a Luvie session is persisted: standalone file,
// NSM (session-manager driven) and LV2 (host driven). The three modes differ
// only in *who owns save timing*, so they share one small interface:
//
//   markDirty() — a real edit happened. Standalone notes it; NSM tells the
//                 session manager (is_dirty); LV2 doesn't go through here at all
//                 (the plugin UI already streams state to the host on each edit).
//   save()      — an explicit save was requested (Ctrl+S, /nsm/client/save, or
//                 the close prompt). Each backend persists in its own way.
//
// collect() builds the full AppState from the live app; it is supplied once by
// the owner so this layer needs no knowledge of the app's widgets.
class SessionManager {
public:
    explicit SessionManager(std::function<AppState()> collect)
        : collect(std::move(collect)) {}
    virtual ~SessionManager() = default;

    virtual void markDirty() = 0;   // edit happened (clean -> dirty)
    virtual bool save() = 0;        // explicit save in the current mode
    virtual bool saveAs() { return false; }   // only Standalone implements

    bool isClean() const { return clean; }

protected:
    std::function<AppState()> collect;
    bool clean = true;
};
