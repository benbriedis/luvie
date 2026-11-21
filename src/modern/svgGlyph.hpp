// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef SVG_GLYPH_HPP
#define SVG_GLYPH_HPP

#include <FL/Fl.H>
#include <FL/Fl_SVG_Image.H>
#include <algorithm>
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
// The artwork carries no colour of its own: either it has no fill at all, which
// nanosvg would render black, or it names "currentColor" wherever the colour
// goes. Either way the wanted colour is put in before rasterising, which lets one
// piece of artwork follow whatever colour its widget draws in.
namespace svgGlyph {

namespace detail {

// Parse `src` and rasterise it `height` pixels tall, the width following from
// the viewBox. (Fl_SVG_Image's own proportional resize fits the artwork inside
// the box it is given, so asking for height x height would shrink anything
// wider than it is tall.)
inline std::unique_ptr<Fl_SVG_Image> rasterise(const std::string& src, int height)
{
	auto img = std::make_unique<Fl_SVG_Image>(nullptr, src.c_str());
	const int W = img->h() > 0 ? (int)(img->w() * (double)height / img->h() + 0.5) : height;
	img->proportional = false;
	img->resize(std::max(1, W), height);
	return img;
}

inline void hex(Fl_Color col, char (&out)[8])
{
	uchar r, g, b;
	Fl::get_color(col, r, g, b);
	std::snprintf(out, sizeof out, "#%02x%02x%02x", r, g, b);
}

}

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

	char hex[8];
	detail::hex(col, hex);

	std::string src = svg;
	static const std::string kCurrent = "currentColor";
	if (src.find(kCurrent) != std::string::npos) {
		// Artwork that says where the colour goes -- needed once it strokes
		// lines or leaves a shape unfilled -- names it "currentColor", as an
		// icon in a web page would. nanosvg knows no such colour, so it is
		// swapped for the wanted one here.
		for (size_t p = 0; (p = src.find(kCurrent, p)) != std::string::npos; p += 7)
			src.replace(p, kCurrent.size(), hex);
	} else {
		// Otherwise give every shape the wanted fill. Each needle ends in a
		// space, so appending keeps the tag name intact.
		for (const char* tag : {"<path ", "<circle "}) {
			std::string needle = tag;
			std::string repl = needle + "fill=\"" + hex + "\" ";
			for (size_t p = 0; (p = src.find(needle, p)) != std::string::npos; p += repl.size())
				src.replace(p, needle.size(), repl);
		}
	}

	return (cache[key] = detail::rasterise(src, height)).get();
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


// Circles. These need no artwork: each is generated at exactly the size asked
// for, so that a ring's stroke stays the given number of pixels however large
// the circle. Both take the same box fl_pie() does -- a d x d square whose
// top-left corner is (x, y) -- and keep inside it: the disc fills it, and the
// ring's outer edge is its edge, the stroke lying inside, as a CSS border does.
namespace detail {

inline Fl_SVG_Image* circle(int d, double stroke, Fl_Color col)
{
	// Keyed on the stroke in hundredths: 0 for a disc.
	static std::map<std::tuple<int, int, unsigned>, std::unique_ptr<Fl_SVG_Image>> cache;
	auto key = std::make_tuple(d, (int)(stroke * 100 + 0.5), (unsigned)col);
	auto it = cache.find(key);
	if (it != cache.end()) return it->second.get();

	char hex[8];
	detail::hex(col, hex);
	char src[256];
	const double c = d / 2.0;
	if (stroke > 0)
		std::snprintf(src, sizeof src,
			R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d"><circle cx="%g" cy="%g" r="%g" fill="none" stroke="%s" stroke-width="%g"/></svg>)",
			d, d, c, c, c - stroke / 2, hex, stroke);
	else
		std::snprintf(src, sizeof src,
			R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d"><circle cx="%g" cy="%g" r="%g" fill="%s"/></svg>)",
			d, d, c, c, c, hex);
	return (cache[key] = rasterise(src, d)).get();
}

}

// A filled circle in the d x d box at (x, y).
inline void disc(int x, int y, int d, Fl_Color col)
{
	if (d > 0) detail::circle(d, 0, col)->draw(x, y);
}

// A circle outline `stroke` pixels wide, its outer edge the d x d box at (x, y).
inline void ring(int x, int y, int d, double stroke, Fl_Color col)
{
	if (d > 0 && stroke > 0) detail::circle(d, stroke, col)->draw(x, y);
}

}

#endif
