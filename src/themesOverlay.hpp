// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "overlayWindow.hpp"
#include "modernChoice.hpp"
#include "theme.hpp"
#include <FL/fl_draw.H>

// Theme picker, opened from the Settings gear menu. A single "Theme" dropdown
// listing themes(); choosing one applies it straight away, leaving the overlay
// open so the themes can be compared.
class ThemesOverlay : public OverlayWindow {
    static constexpr int pad       = 16;
    static constexpr int fieldTopY = headerH + 20;
    static constexpr int labelW    = 76;
    static constexpr int gap       = 8;
    static constexpr int choiceW   = 200;
    static constexpr int choiceH   = 26;
    static constexpr int choiceX   = pad + labelW + gap;

    ModernChoice* themeChoice_ = nullptr;

    void drawStaticContent(int sy, int /*sbW*/) override {
        fl_font(FL_HELVETICA, 12);
        fl_color(textCol());
        fl_draw("Theme", pad, fieldTopY - sy, labelW, choiceH,
                FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    }

public:
    ThemesOverlay(int x, int y, int w, int h)
        : OverlayWindow(x, y, w, h, "Themes")
    {
        begin();
        themeChoice_ = new ModernChoice(choiceX, fieldTopY, choiceW, choiceH);
        themeChoice_->textsize(12);
        for (const Theme* t : themes()) themeChoice_->add(t->name);
        themeChoice_->value(themeIndex());
        themeChoice_->callback([](Fl_Widget* wd, void*) {
            setTheme(static_cast<Fl_Choice*>(wd)->value());
        });
        onTheme([this] {
            themeChoice_->color(inputBgCol());
            themeChoice_->labelcolor(textCol());
            themeChoice_->setBorderColor(borderCol());
            themeChoice_->setArrowColor(subTextCol());
            themeChoice_->setHoverColor(buttonBgCol());
        });
        end();
        hide();
    }

    void show() override {
        themeChoice_->value(themeIndex());
        OverlayWindow::show();
    }
};
