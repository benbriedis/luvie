// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "uiFont.hpp"

#ifdef LUVIE_EMBED_UI_FONT
#include <FL/Fl.H>
#include <FL/Enumerations.H>
#include <fontconfig/fontconfig.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdio>
#include "uiFontData.hpp"

namespace {

// Fontconfig, Xft and HarfBuzz all load fonts from files, not from memory, so each
// embedded face becomes an anonymous in-memory file (memfd) named by its
// /proc/self/fd path. Nothing is written to disk or left behind, and nothing depends
// on where Luvie is installed. The descriptor stays open for the life of the process:
// the font is re-opened through that path whenever a size of it is first drawn.
//
// Registered as an application font of the current fontconfig configuration, so it is
// private to this process -- including when the process is a plugin host, where
// it is the host's configuration that gains a "Luvie Sans" no one else asks for.
bool addFace(const uiFontData::Face& face, const char* name)
{
	const int fd = memfd_create(name, MFD_CLOEXEC);
	if (fd < 0) return false;
	std::size_t done = 0;
	while (done < face.size) {
		const ssize_t n = write(fd, face.data + done, face.size - done);
		if (n <= 0) { close(fd); return false; }
		done += static_cast<std::size_t>(n);
	}
	char path[32];
	std::snprintf(path, sizeof path, "/proc/self/fd/%d", fd);
	if (!FcConfigAppFontAddFile(nullptr, reinterpret_cast<const FcChar8*>(path))) {
		close(fd);
		return false;
	}
	return true;
}

}
#endif

void uiFont::install()
{
#ifdef LUVIE_EMBED_UI_FONT
	static bool installed = false;
	if (installed) return;
	installed = true;

	if (!addFace(uiFontData::regularFace, "LuvieSans-Regular")
	    || !addFace(uiFontData::boldFace, "LuvieSans-Bold")) {
		std::fprintf(stderr, "luvie: couldn't load the built-in UI font; using the system sans\n");
		return;
	}
	// The font name syntax is the text driver's: Pango takes a font description, plain
	// Xft a family behind a one-character style prefix (' ' regular, 'B' bold).
#ifdef LUVIE_HAVE_PANGO
	Fl::set_font(FL_HELVETICA,      "Luvie Sans");
	Fl::set_font(FL_HELVETICA_BOLD, "Luvie Sans Bold");
#else
	Fl::set_font(FL_HELVETICA,      " Luvie Sans");
	Fl::set_font(FL_HELVETICA_BOLD, "BLuvie Sans");
#endif
#endif
}
