// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

// nsmStub.cpp — the build of NsmClient used when NSM support is compiled out.
//
// NSM support is on by default everywhere, so this file is only reached by a build
// configured with -DLUVIE_NSM=OFF — someone who wants neither the feature nor liblo
// linked into the result. src/CMakeLists.txt compiles nsm.cpp otherwise.
//
// It reports "no session manager", which is the state the app already handles: it is
// what happens on Linux whenever NSM_URL is unset. So every caller takes the standalone
// path with no special-casing, and isActive() being a compile-time-known false lets the
// optimizer drop the rest.
//
// Providing the same class with the same signatures, inert, is what keeps main.cpp and
// NsmSession free of #ifdefs around every NSM call site.
//
// Keep this in step with nsm.hpp: a method added there needs an inert twin here, or the
// LUVIE_NSM=OFF link fails.

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
