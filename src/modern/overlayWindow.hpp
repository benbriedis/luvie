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
    // Shared layout + colour constants (also used by subclass content code).
    static constexpr int headerH    = 52;
    static constexpr int scrollbarW = 14;
    static constexpr int titlePad   = 16;
    static constexpr int closeSz    = 36;
    static constexpr int closePad   = 10;

    static constexpr Fl_Color bgCol      = 0xFFFFFF00;
    static constexpr Fl_Color borderCol  = 0xCBD5E100;
    static constexpr Fl_Color dividerCol = 0xE5E7EB00;
    static constexpr Fl_Color textCol    = 0x37415100;
    static constexpr Fl_Color subTextCol = 0x6B728000;

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
