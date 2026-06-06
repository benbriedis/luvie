#pragma once
#include "overlayWindow.hpp"

// Transport settings overlay. Shares its title bar, sizing and scrollbar with
// OverlayWindow; the body is intentionally empty for now.
class TransportOverlay : public OverlayWindow {
public:
    TransportOverlay(int x, int y, int w, int h)
        : OverlayWindow(x, y, w, h, "Transport")
    {
        hide();
    }
};
