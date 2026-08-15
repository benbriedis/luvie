// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "port.hpp"

// A port routed to one of the LV2 plugin's own MIDI outputs. Those outputs exist
// only when Luvie is hosted, where the DSP does the sequencing and there is no
// PortRegistry at all — so in the standalone app this port is deliberately silent.
// It still exists so that a project written in a host keeps its ports, its
// instrument assignments and its "Plugin" setting when opened standalone.
class PluginOutPort : public Port {
public:
    explicit PluginOutPort(std::string name) : Port(std::move(name)) {}

    MidiBackend backend()       const override { return MidiBackend::Plugin; }
    bool        softSequenced() const override { return false; }

    void noteOn (int, int, int) override {}
    void noteOff(int, int)      override {}
    void cc     (int, int, int) override {}
    void pitchBend(int, int)    override {}
    void programChange(int, int, int, int) override {}
};
