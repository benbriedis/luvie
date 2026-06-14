#pragma once
#include <string>
#include "sessionManager.hpp"

// Standalone mode: Luvie owns the project file. Save writes the chosen .luv;
// Save As (or the first Save of an unnamed project) prompts for a path.
//
// markDirty() currently only flips the clean flag — disk auto-save is a planned
// follow-up. The hook is in place so the AutoSaver work can land here later.
class StandaloneSession : public SessionManager {
public:
    StandaloneSession(std::function<AppState()> collect, std::string projectPath);

    void markDirty() override { clean = false; }
    bool save() override;
    bool saveAs() override;

    const std::string& path() const { return projectPath; }

private:
    std::string pickSavePath();                 // Save As dialog; "" if cancelled
    bool writeTo(const std::string& path);      // collect() + saveAppState

    std::string projectPath;   // current .luv path; empty until first save
};
