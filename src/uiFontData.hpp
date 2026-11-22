// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef UI_FONT_DATA_HPP
#define UI_FONT_DATA_HPP

#include <cstddef>

// The bytes of Luvie Sans, compiled in. Defined by the uiFontData.cpp that
// src/CMakeLists.txt generates from src/fonts/LuvieSans-*.ttf, and only in builds
// that embed the font (LUVIE_EMBED_UI_FONT). See uiFont.hpp.
namespace uiFontData {

struct Face {
	const unsigned char* data;
	std::size_t          size;
};

extern const Face regularFace;
extern const Face boldFace;

}

#endif
