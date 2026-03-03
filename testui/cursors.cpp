#include "cursors.hpp"
#include <cmath>

const Fl_RGB_Image* forbiddenCursorImage() {
	static const int SIZE = 20;
	static uchar pix[SIZE * SIZE * 4] = {};
	static Fl_RGB_Image* img = nullptr;
	if (img) return img;

	const float cx          = (SIZE - 1) / 2.0f;
	const float cy          = (SIZE - 1) / 2.0f;
	const float outer_r     = 9.5f;
	const float inner_r     = 7.5f;   // 2px thick ring
	const float slash_width = 1.5f;   // half-width of diagonal slash

	/* Pass 1: record which pixels are part of the cursor shape */
	bool isCursor[SIZE * SIZE] = {};
	for (int y = 0; y < SIZE; y++) {
		for (int x = 0; x < SIZE; x++) {
			float dist = std::sqrt((x - cx)*(x - cx) + (y - cy)*(y - cy));
			bool onCircle = dist >= inner_r && dist <= outer_r;
			bool onSlash  = std::fabs((float)(x - y)) <= slash_width && dist <= outer_r;
			isCursor[y * SIZE + x] = onCircle || onSlash;
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
