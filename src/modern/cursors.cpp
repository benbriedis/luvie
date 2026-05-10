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

const Fl_RGB_Image* contextMenuCursorImage() {
	static const int W = 20;
	static const int H = 22;
	static uchar pix[W * H * 4] = {};
	static Fl_RGB_Image* img = nullptr;
	if (img) return img;

	const float scaleX = (float)W / 17.0f;
	const float scaleY = (float)H / 19.0f;

	auto pip = [](float px, float py, const float* xs, const float* ys, int n) -> bool {
		bool inside = false;
		for (int i = 0, j = n-1; i < n; j = i++) {
			if (((ys[i] > py) != (ys[j] > py)) &&
				(px < (xs[j]-xs[i])*(py-ys[i])/(ys[j]-ys[i])+xs[i]))
				inside = !inside;
		}
		return inside;
	};

	// Inner black arrow body and tail polygons (from SVG)
	const float aiX[] = {1.0f, 1.0f, 3.969f, 4.397f, 9.165f};
	const float aiY[] = {2.814f, 14.002f, 11.136f, 10.997f, 10.997f};
	const float tiX[] = {7.751f, 5.907f, 2.807f, 4.648f};
	const float tiY[] = {16.416f, 17.190f, 9.816f, 9.041f};

	bool black[W * H] = {};
	bool white[W * H] = {};

	for (int y = 0; y < H; y++) {
		for (int x = 0; x < W; x++) {
			float svgX = (x + 0.5f) / scaleX;
			float svgY = (y + 0.5f) / scaleY;
			int i = y * W + x;

			bool inMenuOuter = svgX >= 8 && svgX <= 17 && svgY >= 8 && svgY <= 18;
			bool inMenuInner = svgX >= 9 && svgX <= 16 && svgY >= 9 && svgY <= 17;
			bool inMenuLine  = inMenuInner && (
				(svgY >= 11 && svgY <= 12) || (svgY >= 13 && svgY <= 14) || (svgY >= 15 && svgY <= 16));

			if (inMenuLine) {
				white[i] = true;
			} else if (inMenuInner) {
				black[i] = true;
			} else if (inMenuOuter) {
				white[i] = true;
			} else if (pip(svgX, svgY, aiX, aiY, 5) || pip(svgX, svgY, tiX, tiY, 4)) {
				black[i] = true;
			}
		}
	}

	// White border: transparent pixels adjacent to black → white
	for (int y = 0; y < H; y++) {
		for (int x = 0; x < W; x++) {
			int i = y * W + x;
			if (black[i] || white[i]) continue;
			bool nearBlack = false;
			for (int dy = -1; dy <= 1 && !nearBlack; dy++)
				for (int dx = -1; dx <= 1 && !nearBlack; dx++) {
					int nx = x+dx, ny = y+dy;
					if (nx >= 0 && nx < W && ny >= 0 && ny < H)
						nearBlack = black[ny * W + nx];
				}
			if (nearBlack) white[i] = true;
		}
	}

	for (int i = 0; i < W * H; i++) {
		int idx = i * 4;
		if (black[i])      { pix[idx]=0;   pix[idx+1]=0;   pix[idx+2]=0;   pix[idx+3]=255; }
		else if (white[i]) { pix[idx]=255; pix[idx+1]=255; pix[idx+2]=255; pix[idx+3]=255; }
	}

	img = new Fl_RGB_Image(pix, W, H, 4);
	return img;
}
