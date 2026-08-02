// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#ifndef TRANSPOSE_POPUP_HPP
#define TRANSPOSE_POPUP_HPP

#include "modern/contextMenuPopup.hpp"
#include "modern/modernSpinner.hpp"
#include <FL/Fl_Box.H>
#include <functional>

// Opened by the note context menu's Transpose item, in place of that menu: a
// spinner for the amount plus Cancel/Ok. What the amount counts differs per
// editor — semitones in the pianoroll, GUI rows in the harmony editor — so the
// label is fixed at construction and the range comes from the caller on each
// open, since it depends on where that pattern's notes currently sit.
class TransposePopup : public ContextMenuPopup {
public:
    static constexpr int popupW = 190;

    // Amounts the spinner may offer; always spans 0.
    struct Range { int lo; int hi; };

    explicit TransposePopup(const char* amountLabel)
        : ContextMenuPopup(popupW, pad + headingH + rowH + pad + buttonH + pad)
    {
        auto* heading = new Fl_Box(pad, pad, popupW - 2 * pad, headingH, "Transpose");
        heading->box(FL_NO_BOX);
        heading->labelcolor(popupText);
        heading->labelfont(FL_HELVETICA_BOLD);
        heading->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        const int rowY = pad + headingH;
        auto* label = new Fl_Box(pad, rowY, labelW, rowH, amountLabel);
        label->box(FL_NO_BOX);
        label->labelcolor(popupText);
        label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        const int sx = pad + labelW + labelGap;
        amount = new ModernSpinner(sx, rowY, popupW - pad - sx, rowH);
        amount->step(1);
        amount->value(0);
        amount->setPalette(popupInputBg, popupText, 0xE5E7EB00);

        const int by = popH - pad - buttonH;
        const int bw = (popupW - 3 * pad) / 2;
        auto* cancel = new ModernButton(pad, by, bw, buttonH, "Cancel");
        cancel->color(popupBg);
        cancel->labelcolor(popupText);
        auto* ok = new ModernButton(popupW - pad - bw, by, bw, buttonH, "Ok");
        ok->color(popupBg);
        ok->labelcolor(popupText);

        cancel->callback([](Fl_Widget*, void* d) {
            static_cast<TransposePopup*>(d)->dismiss();
        }, this);
        ok->callback([](Fl_Widget*, void* d) {
            auto* self = static_cast<TransposePopup*>(d);
            auto  fn   = self->onApplyFn;
            int   amt  = (int)self->amount->value();
            self->dismiss();
            if (fn && amt != 0) fn(amt);
        }, this);

        end();
        hide();
    }

    // onApply runs when Ok is pressed, with the amount the spinner holds.
    void open(int wx, int wy, Range limit, std::function<void(int)> onApply) {
        onApplyFn = std::move(onApply);
        amount->range(limit.lo, limit.hi);
        amount->value(0);
        openAt(wx, wy);
    }

private:
    static constexpr int pad      = 8;
    static constexpr int headingH = 20;
    static constexpr int rowH     = 24;
    static constexpr int buttonH  = 26;
    static constexpr int labelW   = 76;
    static constexpr int labelGap = 6;   // breathing room before the spinner

    ModernSpinner*           amount;
    std::function<void(int)> onApplyFn;

    void dismiss() {
        hide();
        if (auto* win = window()) win->redraw();
    }
};

#endif
