// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef UI_FONT_HPP
#define UI_FONT_HPP

// Luvie's own UI font. On Linux the binaries carry Luvie Sans -- Noto Sans cut down
// to Latin (tools/make-ui-font.py) -- so text looks the same whatever fonts the
// distribution happens to install. install() makes it
// available to this process and points FL_HELVETICA and FL_HELVETICA_BOLD, the two
// fonts the UI draws with, at it. Characters it lacks (Greek, Korean... in a user's
// track name) fall back to the system's fonts through Pango.
//
// Elsewhere install() does nothing and FLTK's defaults (Arial, Helvetica) stand.
namespace uiFont {

// Call once, before anything is drawn: from main() and from the plugin UI's
// instantiate(). Later calls do nothing. If the font cannot be loaded, the UI keeps
// the system sans and a warning goes to stderr.
void install();

}

#endif
