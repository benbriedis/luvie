// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "cursors.hpp"
#include <FL/Fl_SVG_Image.H>

// The cursors are drawn from artwork that carries its own colours, unlike the
// svgGlyph artwork, and whose viewBox is its size in pixels. Each is
// rasterised once and handed back as a plain RGB image holding the pixels. It
// must not be the Fl_SVG_Image itself: setting a cursor copies the image first,
// and an Fl_SVG_Image's copy is a fresh, not yet rasterised SVG image with no
// pixels in it, which the X11 driver then reads through a null pointer.
static Fl_RGB_Image* rasteriseCursor(const char* svgText)
{
	Fl_SVG_Image svg(nullptr, svgText);
	svg.normalize();
	Fl_RGB_Image pixels((const uchar*)svg.data()[0], svg.w(), svg.h(), svg.d(), svg.ld());
	return (Fl_RGB_Image*)pixels.copy();   // a deep copy, owning its pixels
}

// The forbidden cursor: a red ring and slash on a white outline, so it reads
// over light and dark alike. The artwork is src/icons/forbiddenCursor.svg,
// minus the XML declaration; re-copy it after editing the file, as this
// constant is what gets compiled in.
static const char* const kForbiddenCursorSvg =
	R"SVG(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="0 0 20 20"><g fill="none" stroke="#ffffff"><circle cx="10" cy="10" r="7.5" stroke-width="4.5"/><path stroke-width="5" d="M4.7 4.7 L15.3 15.3"/></g><g fill="none" stroke="#dc5050"><circle cx="10" cy="10" r="7.5" stroke-width="2.25"/><path stroke-width="3" d="M4.7 4.7 L15.3 15.3"/></g></svg>)SVG";

const Fl_RGB_Image* forbiddenCursorImage() {
	static Fl_RGB_Image* img = rasteriseCursor(kForbiddenCursorSvg);
	return img;
}

// How long the forbidden cursor stays up. Long enough to be read as an answer to
// the key that was just pressed, short enough not to leave the pointer looking
// broken.
static constexpr double forbiddenFlashSecs = 1.0;

// The window the flash is running on, so a second flash replaces the first
// rather than letting the older timeout clear the newer cursor.
static Fl_Window* flashWindow = nullptr;

static void endForbiddenFlash(void*)
{
	if (flashWindow) flashWindow->cursor(FL_CURSOR_DEFAULT);
	flashWindow = nullptr;
}

void flashForbiddenCursor(Fl_Window* win)
{
	if (!win) return;
	Fl::remove_timeout(endForbiddenFlash);
	flashWindow = win;
	win->cursor(forbiddenCursorImage(), kForbiddenCursorHotX, kForbiddenCursorHotY);
	Fl::add_timeout(forbiddenFlashSecs, endForbiddenFlash);
}

// The context-menu cursor: an arrow with a little menu badge, black on a white
// outline. The artwork is src/icons/contextCursor.svg, minus the XML
// declaration; re-copy it after editing the file, as this constant is what gets
// compiled in. Its viewBox being its size in pixels keeps the badge's lines on
// whole pixels.
static const char* const kContextCursorSvg =
	R"SVG(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="0 0 20 22"><g fill="#ffffff" stroke="#ffffff" stroke-width="2.5" stroke-linejoin="round"><path d="M1.5 1.5 L1.5 14.93 L5.06 11.49 L5.58 11.32 L11.3 11.32 Z"/><path d="M9.6 18.82 L7.39 19.75 L3.67 10.9 L5.88 9.97 Z"/></g><g fill="#000000"><path d="M1.5 1.5 L1.5 14.93 L5.06 11.49 L5.58 11.32 L11.3 11.32 Z"/><path d="M9.6 18.82 L7.39 19.75 L3.67 10.9 L5.88 9.97 Z"/></g><rect fill="#ffffff" x="9" y="9" width="11" height="13" rx="1"/><rect fill="#000000" x="10" y="10" width="9" height="11"/><rect fill="#ffffff" x="11" y="12" width="7" height="1"/><rect fill="#ffffff" x="11" y="15" width="7" height="1"/><rect fill="#ffffff" x="11" y="18" width="7" height="1"/></svg>)SVG";

const Fl_RGB_Image* contextMenuCursorImage() {
	static Fl_RGB_Image* img = rasteriseCursor(kContextCursorSvg);
	return img;
}
