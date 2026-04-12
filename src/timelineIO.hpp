#pragma once
#include "timeline.hpp"
#include <string>

// App-level state that gets persisted to / loaded from disk.
struct AppState {
    Timeline timeline;
    int  rootPitch = 0;
    int  chordType = 0;
    bool sharp     = true;
};

// Save/load the full AppState to/from a JSON file.
// Return true on success.
bool saveAppState(const AppState& state, const std::string& filePath);
bool loadAppState(const std::string& filePath, AppState& state);
