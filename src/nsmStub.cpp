// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

// nsmStub.cpp — the non-Linux build of NsmClient.
//
// NSM (New/Non Session Manager) is a Linux session-management protocol spoken over OSC,
// and liblo exists in this project purely to speak it. macOS and Windows have nothing on
// the other end of that conversation, so neither the protocol nor the library is built
// there: src/CMakeLists.txt compiles nsm.cpp on Linux and this file everywhere else.
//
// Rather than wrap every NSM call site in main.cpp in #ifdefs, this provides the same
// class with the same signatures, inert. init() reports "no session manager", which is
// the exact state the app already handles — it is what happens on Linux whenever NSM_URL
// is unset — so every caller takes the standalone path with no special-casing, and
// isActive() being a compile-time-known false lets the optimizer drop the rest.
//
// Keep this in step with nsm.hpp: a method added there needs an inert twin here, or the
// non-Linux link fails.

#include "nsm.hpp"

NsmClient::NsmClient() = default;
NsmClient::~NsmClient() = default;

bool NsmClient::init(const char* /*appName*/, const char* /*exe*/) {
    return false;   // leaves active_ false: "not running under a session manager"
}

void NsmClient::setGuiVisible(bool /*visible*/) {}
void NsmClient::sendDirty() {}
void NsmClient::sendClean() {}
void NsmClient::poll(int /*timeoutMs*/) {}
