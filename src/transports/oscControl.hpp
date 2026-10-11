// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef OSC_CONTROL_HPP
#define OSC_CONTROL_HPP

#include "itransport.hpp"
#include <functional>

/*
 * OscControl — a command-only ITransport that drives a host's transport over OSC,
 * for the LV2 UI when no JACK server carries transport commands. One subclass per
 * host dialect: OscTransport (Ardour) and CarlaOscTransport (Carla).
 *
 * The playhead is displayed from the DSP's time:Position, so position() and
 * isPlaying() are not meaningful here.
 *
 * OscObserver drives the liveness side through open() / close() / probe(): a
 * probe the host acknowledges fires onReply, and each command sent fires
 * onCommandSent so the observer can re-check without polling while all is well.
 * Everything here is called on the UI thread, and onReply fires there too.
 */
class OscControl : public ITransport {
public:
    // Target the host's OSC port on localhost and open whatever is needed to hear
    // back from it. Reopens if already open. False when that fails or this build
    // has no OSC support (see OscTransport::available()).
    virtual bool open(int hostPort) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    // Check the host is there; an acknowledgement fires onReply.
    virtual void probe() = 0;

    std::function<void()> onReply;
    std::function<void()> onCommandSent;

    float position()  const override { return 0.0f; }
    bool  isPlaying() const override { return false; }
};

#endif
