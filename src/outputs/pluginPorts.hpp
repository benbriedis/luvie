// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "midiBackend.hpp"
#include "timelineIO.hpp"
#include <string>
#include <vector>

/*
 * Which of the LV2 plugin's MIDI output ports a Luvie output port drives.
 *
 * LV2 port counts are fixed at build time (they come from the TTL, not from the
 * running project), so the plugin declares kMaxPluginOutputs MIDI outputs and the
 * project's own ports are mapped onto them: the Nth port whose backend is Plugin
 * drives midi_out_N. That keeps the mapping derivable from the saved state alone,
 * which matters because the DSP has to route with no UI running.
 *
 * The LV2 ports cannot take the project's port names — lv2:name is static — so the
 * host shows them as "MIDI Out 1..N" and the project's names decide only the order.
 *
 * Ports on a backend this mode cannot drive (Jack/Native/Debug while hosted) fall
 * back to output 0, which is what every port did before there was more than one
 * output; a project loaded into a host therefore sounds exactly as it did before,
 * and only ports explicitly set to Plugin fan out to the extra ports.
 */
inline constexpr int kMaxPluginOutputs = 8;

// Name of LV2 MIDI output `idx`. Must match the port's lv2:name in luvie_dsp.ttl,
// because the Outputs panel shows a Plugin-backed port under this name (in place of
// its own) so the row says where the MIDI actually comes out. Display only — the
// port's stored name is untouched and stays its routing key.
inline std::string pluginOutputName(int idx)
{
    return "MIDI Out " + std::to_string(idx + 1);
}

// LV2 MIDI output index (0-based) for `name`; always in [0, kMaxPluginOutputs).
inline int pluginPortIndex(const std::vector<JackOutput>& outs, const std::string& name)
{
    int idx = 0;
    for (const auto& o : outs) {
        if (o.backend != MidiBackend::Plugin) continue;
        if (o.portName == name)
            return idx < kMaxPluginOutputs ? idx : kMaxPluginOutputs - 1;
        idx++;
    }
    return 0;   // unmapped port (or unknown name): the original single output
}
