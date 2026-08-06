// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#pragma once

// The path of the one-shot load/restore handoff file the DSP module writes and the UI
// module reads (see the file headers in luvie_dsp.cpp and luvie_ui.cpp — it is written
// once and never polled; live state travels over the `luvie_state` atom).
//
// Both modules are dlopen'd into the *same* host process, so the process id is what ties
// the two ends together, and both sides must derive the identical name. That is the whole
// reason this lives in a shared header rather than being spelled out twice: the two
// snprintf calls it replaces had to be kept character-for-character in step, across two
// files that are otherwise deliberately independent (the DSP never links luvie_core).

#include <filesystem>
#include <string>

#ifdef _WIN32
#include <windows.h>          // GetCurrentProcessId
#else
#include <unistd.h>           // getpid
#endif

namespace luvie {

// Per-process handoff file. Uses the platform temp directory rather than a hardcoded
// /tmp, which does not exist on Windows and is not the sanctioned scratch location on
// macOS (where each session gets its own private $TMPDIR).
inline std::string stateFilePath()
{
#ifdef _WIN32
    const unsigned long pid = GetCurrentProcessId();
#else
    const unsigned long pid = static_cast<unsigned long>(getpid());
#endif

    // temp_directory_path() consults TMPDIR/TMP/TEMP and can throw if none of them
    // name a real directory. A plugin has no way to report that usefully and must not
    // take the host down, so fall back to the working directory instead.
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::temp_directory_path(ec);
    if (ec) dir = ".";

    return (dir / ("luvie_state_" + std::to_string(pid) + ".json")).string();
}

} // namespace luvie
