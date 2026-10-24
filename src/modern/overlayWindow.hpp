// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "basePopup.hpp"
#include <functional>
#include <string>

class ModernButton;
class GridScrollPane;

// Shared base for the View overlays (Outputs, Transport, …).
// Provides the title bar with its close button, the overlay border/sizing,
// a vertical scrollbar with mouse-wheel scrolling, and the clipped drawing of
// child widgets. Subclasses fill in their own content and override the hooks
// below to relayout / reposition that content.
class OverlayWindow : public BasePopup {
public:
    // Shared layout constants and theme colours (also used by subclass content code).
    static constexpr int headerH    = 52;
    static constexpr int scrollbarW = 14;
    static constexpr int titlePad   = 16;
    static constexpr int closeSz    = 36;
    static constexpr int closePad   = 10;

    static Fl_Color bgCol()       { return theme().overlayBg; }
    static Fl_Color borderCol()   { return theme().overlayBorder; }
    static Fl_Color dividerCol()  { return theme().overlayDivider; }
    static Fl_Color textCol()     { return theme().overlayText; }
    static Fl_Color subTextCol()  { return theme().overlaySubText; }
    static Fl_Color inputBgCol()  { return theme().overlayInputBg; }
    static Fl_Color buttonBgCol() { return theme().overlayButtonBg; }

    OverlayWindow(int x, int y, int w, int h, std::string title);

    void hide() override;

    // Fired from hide() (after subclass cleanup) — wire menu toggle state here.
    std::function<void()> onClose;

protected:
    ModernButton*   closeBtn_     = nullptr;
    GridScrollPane* vScrollbar_   = nullptr;
    int             scrollY_      = 0;
    int             totalContentH_= 0;

    // Subclass hooks ----------------------------------------------------------
    // Reposition scrollable child widgets by `delta` pixels.
    virtual void onScroll(int /*delta*/) {}
    // Relayout content after the overlay is resized.
    virtual void onResized() {}
    // Draw static (non-widget) content within the scrolled content area.
    // `scrollY` is the current scroll offset; `sbW` is the scrollbar width (0 if hidden).
    virtual void drawStaticContent(int /*scrollY*/, int /*sbW*/) {}

    // Scroll machinery shared by all overlays.
    void updateScrollbar();
    void applyScrollY(int newY);

    void resize(int x, int y, int w, int h) override;
    void draw() override;
    int  handle(int event) override;

private:
    std::string title_;
};
