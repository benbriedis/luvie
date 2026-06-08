#pragma once
#include "port.hpp"
#include "timelineIO.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>

class JackTransport;

// Owns the live set of output Ports (one per MIDI Output Port row in the overlay)
// and constructs the right Port subclass for each backend. main.cpp keeps this in
// sync with the overlay via reconcile(); the Playhead looks up Ports via find().
class PortRegistry {
public:
    explicit PortRegistry(JackTransport* jack) : jack_(jack) {}

    // Make the registry match `ports` (by name + backend): create new ports,
    // destroy removed ones, and rebuild ports whose backend changed.
    void reconcile(const std::vector<JackOutput>& ports);

    // Rename in place (preserves the backend resource / its connections).
    void rename(const std::string& oldName, const std::string& newName);

    // Re-register all Jack ports on a freshly (re)opened JACK client.
    void reregisterJack();

    Port* find(const std::string& name) const;
    bool  anyJack() const;
    bool  anySoft() const;   // any Native/Debug port present

    // Applied to every DebugPort as it is created (render pitch → note name).
    std::function<std::string(int)> debugPitchName;

private:
    std::unique_ptr<Port> make(const JackOutput& o);

    JackTransport* jack_;
    std::vector<std::unique_ptr<Port>> ports_;
};
