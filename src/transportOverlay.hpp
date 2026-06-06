#pragma once
#include "overlayWindow.hpp"
#include "modernChoice.hpp"
#include <FL/fl_draw.H>
#include <FL/Fl_Menu_Item.H>
#include <functional>

// Transport settings overlay. Shares its title bar, sizing and scrollbar with
// OverlayWindow. Holds a single "Transport" dropdown selecting the clock
// source. When Luvie runs as an LV2 plugin the transport is fixed to "Host"
// and the other options are greyed out; standalone, "Host" is unavailable.
//
//   index 0 = Host, 1 = Internal, 2 = Jack
//
// onTransportChanged fires when the selection changes (by the user or via
// setSelection). While the host is polling for a JACK server, setJackWaiting
// shows a red status line beneath the dropdown.
class TransportOverlay : public OverlayWindow {
    static constexpr int pad       = 16;
    static constexpr int fieldTopY = headerH + 20;
    static constexpr int labelW    = 76;
    static constexpr int gap       = 8;
    static constexpr int choiceW   = 200;
    static constexpr int choiceH   = 26;
    static constexpr int choiceX   = pad + labelW + gap;
    static constexpr int msgY      = fieldTopY + choiceH + 8;

    static constexpr Fl_Color inputBgCol = 0xF9FAFB00;
    static constexpr Fl_Color waitCol    = 0xDC262600;  // red

    ModernChoice* transportChoice_ = nullptr;
    bool          pluginMode_      = false;
    bool          jackWaiting_     = false;

    void applyMode() {
        if (!transportChoice_) return;
        auto setInactive = [this](int i) {
            transportChoice_->mode(i, transportChoice_->mode(i) | FL_MENU_INACTIVE);
        };
        if (pluginMode_) {
            // Host transport, fixed: select it and grey out the alternatives.
            transportChoice_->value(0);  // Host
            setInactive(1);              // Internal
            setInactive(2);              // Jack
        } else {
            // Standalone: no host to follow, so grey out Host.
            setInactive(0);              // Host
            transportChoice_->value(1);  // Internal
        }
    }

    void drawStaticContent(int sy, int /*sbW*/) override {
        fl_font(FL_HELVETICA, 12);
        fl_color(textCol);
        fl_draw("Transport", pad, fieldTopY - sy, labelW, choiceH,
                FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        if (jackWaiting_) {
            fl_font(FL_HELVETICA, 11);
            fl_color(waitCol);
            fl_draw("Waiting for JACK server\xe2\x80\xa6", choiceX, msgY - sy,
                    w() - choiceX - pad, 16, FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        }
    }

    void onScroll(int delta) override {
        if (transportChoice_)
            transportChoice_->position(transportChoice_->x(),
                                       transportChoice_->y() + delta);
    }

public:
    // Fired with the selected index (0=Host, 1=Internal, 2=Jack).
    std::function<void(int)> onTransportChanged;

    TransportOverlay(int x, int y, int w, int h, bool pluginMode)
        : OverlayWindow(x, y, w, h, "Transport"), pluginMode_(pluginMode)
    {
        begin();
        transportChoice_ = new ModernChoice(choiceX, fieldTopY, choiceW, choiceH);
        transportChoice_->color(inputBgCol);
        transportChoice_->labelcolor(textCol);
        transportChoice_->textsize(12);
        transportChoice_->setBorderColor(borderCol);
        transportChoice_->setArrowColor(subTextCol);
        transportChoice_->setHoverColor(0xF3F4F600);
        transportChoice_->add("Host");
        transportChoice_->add("Internal");
        transportChoice_->add("Jack");
        transportChoice_->callback([](Fl_Widget* wd, void* d) {
            auto* self = static_cast<TransportOverlay*>(d);
            if (self->onTransportChanged)
                self->onTransportChanged(static_cast<Fl_Choice*>(wd)->value());
        }, this);
        applyMode();
        end();

        hide();
    }

    // Programmatically choose a transport; fires onTransportChanged.
    void setSelection(int index) {
        if (transportChoice_) transportChoice_->value(index);
        if (onTransportChanged) onTransportChanged(index);
    }

    int selection() const { return transportChoice_ ? transportChoice_->value() : -1; }

    // Show/hide the red "Waiting for JACK server…" status line.
    void setJackWaiting(bool waiting) {
        if (jackWaiting_ == waiting) return;
        jackWaiting_ = waiting;
        redraw();
    }
};
