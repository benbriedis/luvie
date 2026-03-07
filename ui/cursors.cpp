#include "cursors.hpp"
#include <cmath>

const Fl_RGB_Image* forbiddenCursorImage() {
	static const int SIZE = 20;
	static uchar pix[SIZE * SIZE * 4] = {};
	static Fl_RGB_Image* img = nullptr;
	if (img) return img;

	const float cx      = (SIZE - 1) / 2.0f;
	const float cy      = (SIZE - 1) / 2.0f;
	const float outer_r = 8.5f;   // leave ~1px margin for white border
	const float inner_r = 6.5f;   // 2px thick ring
	const float slash_hw = 2.0f;  // half-width of slash (perpendicular distance)

	const float inv_sqrt2 = 1.0f / std::sqrt(2.0f);

	/* Pass 1: record which pixels are part of the cursor shape */
	bool isCursor[SIZE * SIZE] = {};
	for (int y = 0; y < SIZE; y++) {
		for (int x = 0; x < SIZE; x++) {
			float dx   = x - cx;
			float dy   = y - cy;
			float dist = std::sqrt(dx*dx + dy*dy);

			// Ring: between inner and outer radius
			bool onRing  = dist >= inner_r && dist <= outer_r;

			// Slash: true perpendicular distance from the 45° diagonal (top-left to bottom-right)
			// The diagonal direction is (1,1)/√2; perpendicular is (-1,1)/√2
			// perp distance = |(-dx + dy)| / √2
			float perp   = std::fabs(-dx + dy) * inv_sqrt2;
			bool onSlash = perp <= slash_hw && dist <= outer_r;

			isCursor[y * SIZE + x] = onRing || onSlash;
		}
	}

	/* Pass 2: white border on any non-cursor pixel adjacent to a cursor pixel */
	for (int y = 0; y < SIZE; y++) {
		for (int x = 0; x < SIZE; x++) {
			if (isCursor[y * SIZE + x]) continue;
			bool nearCursor = false;
			for (int dy = -1; dy <= 1 && !nearCursor; dy++)
				for (int dx = -1; dx <= 1 && !nearCursor; dx++) {
					int nx = x + dx, ny = y + dy;
					if (nx >= 0 && nx < SIZE && ny >= 0 && ny < SIZE)
						nearCursor = isCursor[ny * SIZE + nx];
				}
			if (nearCursor) {
				int idx = (y * SIZE + x) * 4;
				pix[idx+0] = 255;
				pix[idx+1] = 255;
				pix[idx+2] = 255;
				pix[idx+3] = 255;
			}
		}
	}

	/* Pass 3: paint the red cursor shape on top */
	for (int y = 0; y < SIZE; y++) {
		for (int x = 0; x < SIZE; x++) {
			if (isCursor[y * SIZE + x]) {
				int idx = (y * SIZE + x) * 4;
				pix[idx+0] = 220;
				pix[idx+1] = 80;
				pix[idx+2] = 80;
				pix[idx+3] = 255;
			}
		}
	}

	img = new Fl_RGB_Image(pix, SIZE, SIZE, 4);
	return img;
}
