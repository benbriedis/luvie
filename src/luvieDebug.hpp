// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef LUVIE_DEBUG_HPP
#define LUVIE_DEBUG_HPP

#include <cstdlib>
#include <cstring>

// Opt-in diagnostics: run with LUVIE_DEBUG=1 to trace transport / MIDI activity on
// stderr. Shared by the DSP and the UI so one env var covers both halves of a
// hosted session, where they can be separate processes.
inline bool luvieDebug()
{
	static const bool on = [] {
		const char* e = getenv("LUVIE_DEBUG");
		return e && *e && strcmp(e, "0") != 0;
	}();
	return on;
}

#endif
