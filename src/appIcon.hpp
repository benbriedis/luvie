// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef APPICON_HPP
#define APPICON_HPP

/* Attach the Luvie logo to every window this process opens, as the window's own
   icon (X11 _NET_WM_ICON, and the Wayland/Windows/macOS equivalents).

   This is separate from, and needed in addition to, the Icon= line in
   luvie.desktop: a desktop entry only supplies an icon once the desktop has
   matched a window back to it by WM_CLASS, and nothing matches when the app runs
   from a build tree or an unpacked tarball with no .desktop installed. FLTK also
   writes _NET_WM_ICON unconditionally — as an *empty* property when no icon was
   set — which GNOME reads as "this window has no icon" and draws a placeholder
   for, so leaving it unset is not neutral.

   Call once, before the first window is shown. */
void setAppIcon();

#endif
