#include "connectionsOverlay.hpp"
#include "modernButton.hpp"
#include <FL/fl_draw.H>

static constexpr int headerH  = 52;
static constexpr int closeSz  = 36;
static constexpr int closePad = 10;
static constexpr int titlePad = 16;

static constexpr Fl_Color bgCol      = 0xFFFFFF00;
static constexpr Fl_Color borderCol  = 0xCBD5E100;
static constexpr Fl_Color dividerCol = 0xE5E7EB00;
static constexpr Fl_Color textCol    = 0x37415100;

ConnectionsOverlay::ConnectionsOverlay(int x, int y, int w, int h)
    : BasePopup(x, y, w, h)
{
    box(FL_NO_BOX);

    // Coordinates are relative to this sub-window (0,0 = top-left of overlay)
    closeBtn = new ModernButton(
        w - closePad - closeSz,
        (headerH - closeSz) / 2,
        closeSz, closeSz, "\xc3\x97");   // UTF-8 × (U+00D7)
    closeBtn->labelsize(22);
    closeBtn->labelcolor(textCol);
    closeBtn->color(bgCol);
    closeBtn->setBorderWidth(0);
    closeBtn->callback([](Fl_Widget*, void* d) {
        static_cast<ConnectionsOverlay*>(d)->hide();
    }, this);

    end();
    hide();
}

void ConnectionsOverlay::hide() {
    if (visible() && onClose) onClose();
    BasePopup::hide();
}

void ConnectionsOverlay::draw() {
    // Background fill
    fl_color(bgCol);
    fl_rectf(0, 0, w(), h());

    // Outer border
    fl_color(borderCol);
    fl_line_style(FL_SOLID, 1);
    fl_rect(0, 0, w(), h());
    fl_line_style(0);

    // Header divider
    fl_color(dividerCol);
    fl_line_style(FL_SOLID, 1);
    fl_line(0, headerH, w(), headerH);
    fl_line_style(0);

    // Title
    fl_font(FL_HELVETICA_BOLD, 17);
    fl_color(textCol);
    fl_draw("Connections", titlePad, 0, w() - titlePad, headerH,
            FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    draw_children();
}
