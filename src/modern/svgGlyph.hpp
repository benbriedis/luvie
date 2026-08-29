// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef SVG_GLYPH_HPP
#define SVG_GLYPH_HPP

#include <FL/Fl.H>
#include <FL/Fl_SVG_Image.H>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <tuple>

// Little pictures drawn from inline SVG artwork. We cannot count on a font that
// has the musical symbols, so the glyphs that would need one (dotted notes, the
// sharp and flat signs) are drawn from artwork instead: minimised copies of the
// files in src/icons/, with the editor cruft, the <style> rules and the
// split-across-lines tag whitespace stripped, so that the fill injection below
// matches and nothing but our own fill decides the colour.
//
// The artwork carries no fill, so nanosvg would render it black; the wanted
// colour is injected before rasterising, which lets one piece of artwork follow
// whatever colour its widget draws in.
namespace svgGlyph {

// `svg` rasterised `height` pixels tall (the width follows from the viewBox) and
// tinted to `col`. Widgets repaint often -- on hover, and on every menu open and
// close -- and re-parsing the SVG each paint would be wasteful, so the results
// are cached. `svg` must be a long-lived string (a literal): the cache keys on
// its address, not its text.
inline Fl_SVG_Image* image(const char* svg, Fl_Color col, int height)
{
	// The cache owns the images: a bare pointer here leaks every glyph at exit,
	// since clearing the map frees its nodes but not what they point at.
	static std::map<std::tuple<const char*, unsigned, int>,
	                std::unique_ptr<Fl_SVG_Image>> cache;

	auto key = std::make_tuple(svg, (unsigned)col, height);
	auto it = cache.find(key);
	if (it != cache.end()) return it->second.get();

	uchar r, g, b;
	Fl::get_color(col, r, g, b);
	char hex[8];
	std::snprintf(hex, sizeof hex, "#%02x%02x%02x", r, g, b);

	// Give every shape the wanted fill. Each needle ends in a space, so appending
	// keeps the tag name intact.
	std::string src = svg;
	for (const char* tag : {"<path ", "<circle "}) {
		std::string needle = tag;
		std::string repl = needle + "fill=\"" + hex + "\" ";
		for (size_t p = 0; (p = src.find(needle, p)) != std::string::npos; p += repl.size())
			src.replace(p, needle.size(), repl);
	}

	auto img = std::make_unique<Fl_SVG_Image>(nullptr, src.c_str());
	img->resize(height, height);   // proportional: aspect < 1 sets the width
	return (cache[key] = std::move(img)).get();
}

// The width `svg` takes at `height` pixels tall. The colour cannot change the
// geometry, so every measurement shares the one cache entry this asks for.
inline int width(const char* svg, int height)
{
	return image(svg, FL_BLACK, height)->w();
}

// Draw `svg` centred on (cx, cy), `height` pixels tall and tinted to `col`.
inline void draw(const char* svg, int cx, int cy, Fl_Color col, int height)
{
	Fl_SVG_Image* img = image(svg, col, height);
	img->draw(cx - img->w() / 2, cy - img->h() / 2);
}

}

#endif
