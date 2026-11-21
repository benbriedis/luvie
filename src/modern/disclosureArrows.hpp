// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef DISCLOSURE_ARROWS_HPP
#define DISCLOSURE_ARROWS_HPP

// The arrows that fold and unfold things -- the Song Editor's tracks, the
// collapsible panes: pointing down when open, right when folded. The artwork is
// src/icons/disclosureDown.svg and disclosureRight.svg, minus the XML
// declaration. Re-copy them after editing the files: these constants are what
// get compiled in. Each has an odd extent across its point, so drawn at its
// design size (8 and 9 pixels tall) the point lands mid-pixel.
// Draw them with svgGlyph::draw().
inline const char* kDisclosureDownSvg =
	R"SVG(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="0 0 9 8"><path fill="currentColor" stroke="currentColor" stroke-width="1" stroke-linejoin="round" d="M1 1.5 L8 1.5 L4.5 6.5 Z"/></svg>)SVG";
inline const char* kDisclosureRightSvg =
	R"SVG(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" viewBox="0 0 8 9"><path fill="currentColor" stroke="currentColor" stroke-width="1" stroke-linejoin="round" d="M1.5 1 L1.5 8 L6.5 4.5 Z"/></svg>)SVG";

#endif
