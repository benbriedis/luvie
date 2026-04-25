#pragma once
#include "timeline.hpp"
#include <map>
#include <string>
#include <vector>

struct JackConnection {
    std::string portName; // name of our registered JACK output port
};

struct JackChannel {
    std::string name;
    std::string portName;
    int         midiChannel = 1;
    std::map<int, std::string> drumMap;
};

// App-level state that gets persisted to / loaded from disk.
struct AppState {
    Timeline timeline;
    int  rootPitch = 0;
    int  chordType = 0;
    bool sharp     = true;
    std::vector<JackConnection> jackConnections;
    std::vector<JackChannel>    jackChannels;
};

// Save/load the full AppState to/from a JSON file.
// Return true on success.
bool saveAppState(const AppState& state, const std::string& filePath);
bool loadAppState(const std::string& filePath, AppState& state);
