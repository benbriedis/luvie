// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "port.hpp"
#include "timelineIO.hpp"
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class JackTransport;

// Owns the live set of output Ports (one per MIDI Output Port row in the overlay)
// and constructs the right Port subclass for each backend. main.cpp keeps this in
// sync with the overlay via reconcile(); UI code looks up Ports via find().
//
// Threads: everything here runs on the UI thread except sendRaw(), which the
// MidiScheduler's sender thread calls. The set is changed only on the UI thread, so
// UI-side reads need no lock; the changes and sendRaw() share mutex_, so a port is
// never destroyed or renamed in the middle of a send.
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

    // Send one raw message on the named port, if it still exists. Sender thread.
    void  sendRaw(const char* name, const uint8_t* data, int len);
    bool  anyJack() const;
    bool  anyNonJack() const;   // a Native or Debug port: sent by the MidiScheduler

    // Applied to every DebugPort as it is created (render pitch → note name).
    std::function<std::string(int)> debugPitchName;

private:
    std::unique_ptr<Port> make(const JackOutput& o);

    JackTransport* jack_;
    std::vector<std::unique_ptr<Port>> ports_;
    mutable std::mutex mutex_;
};
