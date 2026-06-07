#pragma once
#include "overlayWindow.hpp"
#include "modernChoice.hpp"
#include "modernButton.hpp"
#include "midiBackend.hpp"
#include <FL/fl_draw.H>
#include <FL/Fl_Menu_Item.H>
#include <functional>

// Shown once when Luvie opens a brand-new (unsaved) project. It lets the user
// pick the two settings that are awkward to change after the fact:
//   • the transport clock source   (mirrors the Transport overlay)
//   • the default MIDI output type  (mirrors the Outputs overlay's "Default port type")
//
// Selections apply live via onTransportChanged / onDefaultBackendChanged; the
// "Create Project" button (and the close box) simply dismiss the dialog.
//
//   transport index 0 = Host, 1 = Internal, 2 = Jack
//   backend         0 = Jack, 1 = Native,   2 = Debug
class StartupOverlay : public OverlayWindow {
    static constexpr int pad     = 16;
    static constexpr int labelW  = 150;
    static constexpr int gap     = 10;
    static constexpr int choiceW = 170;
    static constexpr int choiceH = 26;
    static constexpr int choiceX = pad + labelW + gap;
    static constexpr int msgY    = headerH + 14;
    static constexpr int msgH    = 34;
    static constexpr int row0Y   = msgY + msgH + 8;
    static constexpr int rowGap  = 44;
    static constexpr int row1Y   = row0Y + rowGap;
    static constexpr int btnY    = row1Y + rowGap;
    static constexpr int btnW    = 130;
    static constexpr int btnH    = 30;

    static constexpr Fl_Color inputBgCol = 0xF9FAFB00;

    ModernChoice* transportChoice_ = nullptr;
    ModernChoice* backendChoice_   = nullptr;
    ModernButton* confirmBtn_      = nullptr;
    bool          pluginMode_      = false;

    void drawStaticContent(int sy, int /*sbW*/) override {
        fl_font(FL_HELVETICA, 12);
        fl_color(subTextCol);
        fl_draw("Please select the transport and MIDI output type to use for your "
                "new project.",
                pad, msgY - sy, w() - 2*pad, msgH,
                FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP | FL_ALIGN_INSIDE);

        fl_font(FL_HELVETICA, 12);
        fl_color(textCol);
        fl_draw("Transport", pad, row0Y - sy, labelW, choiceH,
                FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        fl_draw("Default MIDI output", pad, row1Y - sy, labelW, choiceH,
                FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    }

    ModernChoice* makeChoice(int x, int y) {
        auto* c = new ModernChoice(x, y, choiceW, choiceH);
        c->color(inputBgCol);
        c->labelcolor(textCol);
        c->textsize(12);
        c->setBorderColor(borderCol);
        c->setArrowColor(subTextCol);
        c->setHoverColor(0xF3F4F600);
        return c;
    }

public:
    std::function<void(int)>         onTransportChanged;       // 0=Host,1=Internal,2=Jack
    std::function<void(MidiBackend)> onDefaultBackendChanged;  // Jack/Native/Debug

    StartupOverlay(int x, int y, int w, int h, bool pluginMode)
        : OverlayWindow(x, y, w, h, "Project Settings"), pluginMode_(pluginMode)
    {
        // No close box — the only way out is the Confirm Settings button.
        if (closeBtn_) closeBtn_->hide();

        begin();

        transportChoice_ = makeChoice(choiceX, row0Y);
        transportChoice_->add("Host");
        transportChoice_->add("Internal");
        transportChoice_->add("Jack");
        // Standalone has no host to follow; plugin mode is fixed to the host clock.
        auto setInactive = [this](int i) {
            transportChoice_->mode(i, transportChoice_->mode(i) | FL_MENU_INACTIVE);
        };
        if (pluginMode_) { setInactive(1); setInactive(2); transportChoice_->value(0); }
        else             { setInactive(0); transportChoice_->value(2); }
        transportChoice_->callback([](Fl_Widget* wd, void* d) {
            auto* self = static_cast<StartupOverlay*>(d);
            if (self->onTransportChanged)
                self->onTransportChanged(static_cast<Fl_Choice*>(wd)->value());
        }, this);

        backendChoice_ = makeChoice(choiceX, row1Y);
        backendChoice_->add("Jack");
        backendChoice_->add("Native");
        backendChoice_->add("Debug");
        backendChoice_->value(0);
        backendChoice_->callback([](Fl_Widget* wd, void* d) {
            auto* self = static_cast<StartupOverlay*>(d);
            if (self->onDefaultBackendChanged)
                self->onDefaultBackendChanged(
                    static_cast<MidiBackend>(static_cast<Fl_Choice*>(wd)->value()));
        }, this);

        confirmBtn_ = new ModernButton(w - pad - btnW, btnY, btnW, btnH, "Confirm Settings");
        confirmBtn_->labelsize(12);
        confirmBtn_->labelcolor(textCol);
        confirmBtn_->color(0xF3F4F600);
        confirmBtn_->setBorderWidth(1);
        confirmBtn_->setBorderColor(borderCol);
        confirmBtn_->callback([](Fl_Widget*, void* d) {
            static_cast<StartupOverlay*>(d)->hide();
        }, this);

        end();
        hide();
    }

    // Initialise the dropdowns to the app's current defaults (no callbacks fired).
    void setSelections(int transportIndex, MidiBackend backend) {
        if (transportChoice_ && !pluginMode_ && transportIndex >= 0)
            transportChoice_->value(transportIndex);
        if (backendChoice_)
            backendChoice_->value(static_cast<int>(backend));
    }
};
