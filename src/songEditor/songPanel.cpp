// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "songPanel.hpp"
#include "panelStyle.hpp"
#include <iterator>

// Column-width multipliers, in dropdown order. x1 is the width the song grid is
// built at, so it is the default and nothing changes until another is picked.
static constexpr float       kZoomFactors[] = { 1.0f, 0.5f, 0.2f };
static const char* const     kZoomLabels[]  = { "1x", "0.5x", "0.2x" };
static constexpr int         kZoomDefault   = 0;

SongPanel::SongPanel(int x, int y, int w, int h)
    : ControlBar(x, y, w, h),
      zoomChoice(0, 0, zoomChoiceW, ctrlH)
{
    initZoomChoice();
    end();
    relayout();
}

void SongPanel::initZoomChoice()
{
    for (const char* v : kZoomLabels)
        zoomChoice.add(v);
    zoomChoice.value(kZoomDefault);
    // Sitting first in the row, it would otherwise grab the window's initial
    // keyboard focus; drop it from focus navigation (still fully clickable).
    zoomChoice.clear_visible_focus();
    zoomChoice.color(panelBg);
    zoomChoice.labelcolor(panelText);
    zoomChoice.setBorderColor(panelCtrlBorder);
    zoomChoice.tooltip("Horizontal zoom");
    zoomChoice.callback([](Fl_Widget*, void* d) {
        auto* self = static_cast<SongPanel*>(d);
        if (self->onZoomChanged) self->onZoomChanged(self->zoomFactor());
    }, this);
}

float SongPanel::zoomFactor() const
{
    int idx = zoomChoice.value();
    if (idx < 0 || idx >= (int)std::size(kZoomFactors)) idx = kZoomDefault;
    return kZoomFactors[idx];
}

std::vector<PanelRow> SongPanel::buildLayout(int /*availW*/)
{
    PanelRow row;
    row.left = { {&zoomChoice, zoomChoiceW} };
    return { row };
}
