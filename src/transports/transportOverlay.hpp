// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "overlayWindow.hpp"
#include "modernChoice.hpp"
#include "modernValueInput.hpp"
#include <FL/fl_draw.H>
#include <FL/Fl_Menu_Item.H>
#include <functional>
#include <string>

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
//
// Plugin mode adds a "Control" dropdown choosing how Luvie's transport buttons
// reach the host — Jack, Ardour OSC or None — and, for Ardour OSC, the port of
// Ardour's OSC surface. setStatus() shows the chosen mechanism's state in the
// same red line.
//
//   control index 0 = Jack, 1 = Ardour OSC, 2 = None
class TransportOverlay : public OverlayWindow {
public:
    enum Control { ControlJack = 0, ControlArdourOsc = 1, ControlNone = 2 };

private:
    static constexpr int pad       = 16;
    static constexpr int fieldTopY = headerH + 20;
    static constexpr int labelW    = 76;
    static constexpr int gap       = 8;
    static constexpr int choiceW   = 200;
    static constexpr int choiceH   = 26;
    static constexpr int choiceX   = pad + labelW + gap;
    static constexpr int rowStep   = choiceH + 12;
    static constexpr int portW     = 80;

    static constexpr Fl_Color inputBgCol = 0xF9FAFB00;
    static constexpr Fl_Color waitCol    = 0xDC262600;  // red

    ModernChoice*     transportChoice_ = nullptr;
    ModernChoice*     controlChoice_   = nullptr;   // plugin mode only
    ModernValueInput* oscPortInput_    = nullptr;   // plugin mode only
    bool              pluginMode_      = false;
    std::string       status_;

    bool oscPortShown() const {
        return controlChoice_ && controlChoice_->value() == ControlArdourOsc;
    }
    int rowCount() const { return pluginMode_ ? (oscPortShown() ? 3 : 2) : 1; }
    int rowY(int row) const { return fieldTopY + row * rowStep; }

    void styleChoice(ModernChoice* c) {
        c->color(inputBgCol);
        c->labelcolor(textCol);
        c->textsize(12);
        c->setBorderColor(borderCol);
        c->setArrowColor(subTextCol);
        c->setHoverColor(0xF3F4F600);
    }

    void updatePortVisibility() {
        if (!oscPortInput_) return;
        if (oscPortShown()) oscPortInput_->show();
        else                oscPortInput_->hide();
        redraw();
    }

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
        fl_draw("Transport", pad, rowY(0) - sy, labelW, choiceH,
                FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        if (pluginMode_)
            fl_draw("Control", pad, rowY(1) - sy, labelW, choiceH,
                    FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        if (oscPortShown())
            fl_draw("OSC port", pad, rowY(2) - sy, labelW, choiceH,
                    FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        if (!status_.empty()) {
            const int msgY = rowY(rowCount() - 1) + choiceH + 8;
            fl_font(FL_HELVETICA, 11);
            fl_color(waitCol);
            fl_draw(status_.c_str(), choiceX, msgY - sy, w() - choiceX - pad, 32,
                    FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
        }
    }

    void onScroll(int delta) override {
        for (Fl_Widget* wd : {(Fl_Widget*)transportChoice_, (Fl_Widget*)controlChoice_,
                              (Fl_Widget*)oscPortInput_})
            if (wd) wd->position(wd->x(), wd->y() + delta);
    }

public:
    // Fired with the selected index (0=Host, 1=Internal, 2=Jack).
    std::function<void(int)> onTransportChanged;
    // Plugin mode: fired by the user changing the Control dropdown (a Control) or
    // committing a new OSC port. Not fired by setControl() / setOscPort().
    std::function<void(int)> onControlChanged;
    std::function<void(int)> onOscPortChanged;

    TransportOverlay(int x, int y, int w, int h, bool pluginMode)
        : OverlayWindow(x, y, w, h, "Transport"), pluginMode_(pluginMode)
    {
        begin();
        transportChoice_ = new ModernChoice(choiceX, rowY(0), choiceW, choiceH);
        styleChoice(transportChoice_);
        transportChoice_->add("Host");
        transportChoice_->add("Internal");
        transportChoice_->add("Jack");
        transportChoice_->callback([](Fl_Widget* wd, void* d) {
            auto* self = static_cast<TransportOverlay*>(d);
            if (self->onTransportChanged)
                self->onTransportChanged(static_cast<Fl_Choice*>(wd)->value());
        }, this);
        applyMode();

        if (pluginMode_) {
            controlChoice_ = new ModernChoice(choiceX, rowY(1), choiceW, choiceH);
            styleChoice(controlChoice_);
            controlChoice_->add("Jack");         // ControlJack
            controlChoice_->add("Ardour OSC");   // ControlArdourOsc
            controlChoice_->add("None");         // ControlNone
            controlChoice_->value(ControlJack);
            controlChoice_->callback([](Fl_Widget* wd, void* d) {
                auto* self = static_cast<TransportOverlay*>(d);
                self->updatePortVisibility();
                if (self->onControlChanged)
                    self->onControlChanged(static_cast<Fl_Choice*>(wd)->value());
            }, this);

            oscPortInput_ = new ModernValueInput(choiceX, rowY(2), portW, choiceH);
            oscPortInput_->color(inputBgCol);
            oscPortInput_->setBorderColor(borderCol);
            oscPortInput_->textcolor(textCol);
            oscPortInput_->cursor_color(textCol);
            oscPortInput_->textsize(12);
            oscPortInput_->range(1, 65535);
            oscPortInput_->step(1);
            oscPortInput_->value(3819);
            oscPortInput_->when(FL_WHEN_RELEASE);
            oscPortInput_->callback([](Fl_Widget* wd, void* d) {
                auto* self = static_cast<TransportOverlay*>(d);
                if (self->onOscPortChanged)
                    self->onOscPortChanged(static_cast<int>(static_cast<Fl_Valuator*>(wd)->value()));
            }, this);
            updatePortVisibility();
        }
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
        setStatus(waiting ? "Waiting for JACK server\xe2\x80\xa6" : "");
    }

    // The red status line beneath the controls; empty hides it.
    void setStatus(const std::string& text) {
        if (status_ == text) return;
        status_ = text;
        redraw();
    }

    // Plugin mode: the transport control mechanism and Ardour's OSC port.
    int  control() const { return controlChoice_ ? controlChoice_->value() : ControlNone; }
    void setControl(int c) {
        if (!controlChoice_ || c < ControlJack || c > ControlNone) return;
        controlChoice_->value(c);
        updatePortVisibility();
    }
    int  oscPort() const { return oscPortInput_ ? static_cast<int>(oscPortInput_->value()) : 0; }
    void setOscPort(int port) { if (oscPortInput_) oscPortInput_->value(port); }
};
