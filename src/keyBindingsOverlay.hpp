// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "overlayWindow.hpp"
#include <FL/fl_draw.H>
#include <iterator>

// Read-only reference list of the keyboard and mouse shortcuts, opened from the
// Settings gear menu. Two columns: the key combination and what it does. The
// bindings are fixed, so the list is drawn rather than built from widgets.
class KeyBindingsOverlay : public OverlayWindow {
    struct Binding { const char* keys; const char* description; };

    static constexpr Binding bindings[] = {
        { "Ctrl-click",       "Select/deselect" },
        { "Shift-click",      "Selection area" },
        { "Alt-click-sweep",  "Slice selection" },
        { "Ctrl-A",           "Select all" },
        { "Ctrl-Z",           "Undo" },
        { "Ctrl-Y",           "Redo" },
        { "Ctrl-C",           "Copy selection" },
        { "Ctrl-X",           "Cut selection" },
        { "Ctrl-V",           "Paste" },
        { "Delete/Backspace", "Delete selection, or the item under the cursor" },
        { "Escape",           "Close menu, or clear selection" },
        { "Space",            "Play/pause" },
        { "<",                "Rewind" },
        { "p",                "Song Editor: move the playhead to the cursor" },
        { "Enter",            "Song/Loop Editor: open the pattern under the cursor" },
    };
    static constexpr int rowCount = (int)std::size(bindings);

    static constexpr int pad      = 16;
    static constexpr int topY     = headerH + 16;
    static constexpr int rowH     = 26;
    static constexpr int keysColW = 180;

    static constexpr Fl_Color headRuleCol = 0xCBD5E100;
    static constexpr Fl_Color stripeCol   = 0xF9FAFB00;

    void drawStaticContent(int sy, int sbW) override {
        const int right = w() - sbW - pad;
        const int descX = pad + keysColW;
        int y = topY - sy;

        // Column headings
        fl_font(FL_HELVETICA_BOLD, 12);
        fl_color(subTextCol);
        fl_draw("Key", pad + 8, y, keysColW - 8, rowH, FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        fl_draw("Operation", descX, y, right - descX, rowH, FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        y += rowH;
        fl_color(headRuleCol);
        fl_line(pad, y - 1, right, y - 1);

        for (int i = 0; i < rowCount; i++, y += rowH) {
            if (i % 2) {
                fl_color(stripeCol);
                fl_rectf(pad, y, right - pad, rowH);
            }
            fl_font(FL_HELVETICA_BOLD, 12);
            fl_color(textCol);
            fl_draw(bindings[i].keys, pad + 8, y, keysColW - 8, rowH,
                    FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
            fl_font(FL_HELVETICA, 12);
            fl_draw(bindings[i].description, descX, y, right - descX, rowH,
                    FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        }
    }

    void onResized() override { updateScrollbar(); }

public:
    KeyBindingsOverlay(int x, int y, int w, int h)
        : OverlayWindow(x, y, w, h, "Key Bindings")
    {
        // Content height measured from the top of the content area.
        totalContentH_ = (topY - headerH) + (rowCount + 1) * rowH + pad;
        updateScrollbar();
        hide();
    }
};
