#pragma once
#include "basePopup.hpp"
#include <functional>

class ModernButton;

class ConnectionsOverlay : public BasePopup {
    ModernButton* closeBtn = nullptr;

    void draw() override;

public:
    void hide() override;
    ConnectionsOverlay(int x, int y, int w, int h);

    std::function<void()> onClose;
};
