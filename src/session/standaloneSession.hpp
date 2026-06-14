#pragma once
#include <string>
#include "sessionManager.hpp"

// Standalone mode: Luvie owns the project file. Save writes the chosen .luv;
// Save As (or the first Save of an unnamed project) prompts for a path.
//
// markDirty() flips the clean flag and arms a debounced auto-save: once a project
// file exists, the document is written back to disk autoSaveDelay seconds after
// the last edit (the timer is reset on every edit). Saving on exit is handled by
// the window-close path in main, which calls save() directly when a path is known.
class StandaloneSession : public SessionManager {
public:
    StandaloneSession(std::function<AppState()> collect, std::string projectPath);
    ~StandaloneSession() override;

    void markDirty() override;
    bool save() override;
    bool saveAs() override;

    const std::string& path() const { return projectPath; }

private:
    std::string pickSavePath();                 // Save As dialog; "" if cancelled
    bool writeTo(const std::string& path);      // collect() + saveAppState

    void scheduleAutoSave();                    // (re)arm the debounce timer
    void autoSave();                            // timer fired: persist if dirty
    static void autoSaveCb(void* self);         // FLTK timeout trampoline

    static constexpr double autoSaveDelay = 10.0;   // seconds after the last edit

    std::string projectPath;   // current .luv path; empty until first save
};
