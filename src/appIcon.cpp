// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "appIcon.hpp"
#include "appIconData.h"

#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Window.H>
#include <memory>
#include <vector>

void setAppIcon()
{
	/* Several sizes rather than one: a window carries a list, and the desktop
	   picks the nearest to what it needs (16px titlebar, ~48px dock, larger for
	   the alt-tab switcher). Handing it one large image instead leaves the
	   downscaling to the compositor, which blurs the small ones. */
	std::vector<std::unique_ptr<Fl_PNG_Image>> images;
	std::vector<const Fl_RGB_Image*>           icons;

	for (const LuvieIconPng& png : luvieIconPngs) {
		auto image = std::make_unique<Fl_PNG_Image>("luvie", png.data, png.bytes);
		// A decode failure would mean a corrupt embedded PNG. Skip it rather than
		// handing FLTK an empty image, which would land in _NET_WM_ICON as a
		// zero-sized entry — the very thing this function exists to avoid.
		if (image->fail())
			continue;
		icons.push_back(image.get());
		images.push_back(std::move(image));
	}

	if (icons.empty())
		return;

	// Copies the pixel data, so the images can go out of scope on return.
	Fl_Window::default_icons(icons.data(), (int)icons.size());
}
